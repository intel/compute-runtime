/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/linux/drm_helper.h"

namespace L0 {

static DebugSessionLinuxPopulateFactory<DEBUG_SESSION_LINUX_TYPE_XE, DebugSessionLinuxXe>
    populateXeDebugger;

DebugSession *createDebugSessionHelperXe(const zet_debug_config_t &config, Device *device, int debugFd, void *params);

DebugSessionLinuxXe::DebugSessionLinuxXe(const zet_debug_config_t &config, Device *device, int debugFd, void *params) : DebugSessionLinux(config, device, debugFd) {
    ioctlHandler.reset(new IoctlHandlerXe);

    if (params) {
        this->xeDebuggerVersion = reinterpret_cast<drm_xe_eudebug_connect *>(params)->version;
    }
};
DebugSessionLinuxXe::~DebugSessionLinuxXe() {

    closeAsyncThread();
    closeInternalEventsThread();
    closeFd();
}

DebugSession *DebugSessionLinuxXe::createLinuxSession(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {

    struct drm_xe_eudebug_connect open = {
        .extensions = 0,
        .pid = config.pid,
        .flags = 0,
        .version = 0};
    auto debugFd = DrmHelper::ioctl(device, NEO::DrmIoctl::debuggerOpen, &open);
    if (debugFd >= 0) {
        PRINT_DEBUGGER_INFO_LOG("drm_xe_eudebug_connect: open.pid: %d, debugFd: %d\n",
                                open.pid, debugFd);

        return createDebugSessionHelperXe(config, device, debugFd, &open);
    } else {
        auto reason = DrmHelper::getErrno(device);
        PRINT_DEBUGGER_ERROR_LOG("drm_xe_eudebug_connect failed: open.pid: %d, retCode: %d, errno: %d\n",
                                 open.pid, debugFd, reason);
        result = translateDebuggerOpenErrno(reason);
    }
    return nullptr;
}

bool DebugSessionLinuxXe::handleInternalEvent() {
    auto eventMemory = getInternalEvent();
    if (eventMemory == nullptr) {
        return false;
    }

    auto debugEvent = reinterpret_cast<drm_xe_eudebug_event *>(eventMemory.get());
    handleEvent(debugEvent);
    return true;
}

void *DebugSessionLinuxXe::asyncThreadFunction(void *arg) {
    DebugSessionLinuxXe *self = reinterpret_cast<DebugSessionLinuxXe *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger async thread start\n", "");

    while (self->asyncThread.threadActive) {
        self->handleEventsAsync();
        self->generateEventsAndResumeStoppedThreads();
        self->sendInterrupts();
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread closing\n", "");

    return nullptr;
}

void DebugSessionLinuxXe::startAsyncThread() {
    asyncThread.thread = NEO::Thread::create(asyncThreadFunction, reinterpret_cast<void *>(this));
}

void DebugSessionLinuxXe::readInternalEventsAsync() {

    struct pollfd pollFd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0,
    };

    int pollTimeout = 1000;
    auto numberOfFds = ioctlHandler->poll(&pollFd, 1, pollTimeout);

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread readEvent poll() retCode: %d\n", numberOfFds);

    if (!detached && numberOfFds < 0 && errno == EINVAL) {
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_DETACHED;
        debugEvent.info.detached.reason = ZET_DEBUG_DETACH_REASON_INVALID;
        PRINT_DEBUGGER_INFO_LOG("Debugger detached\n", "");
        pushApiEvent(debugEvent);
        detached = true;
    } else if (numberOfFds > 0) {

        ze_result_t result = ZE_RESULT_SUCCESS;

        uint8_t maxEventBuffer[sizeof(drm_xe_eudebug_event) + maxEventSize];
        auto event = reinterpret_cast<drm_xe_eudebug_event *>(maxEventBuffer);
        event->len = maxEventSize;
        event->type = DRM_XE_EUDEBUG_EVENT_READ;
        event->flags = 0;

        result = readEventImp(event);

        if (result == ZE_RESULT_SUCCESS) {
            std::lock_guard<std::mutex> lock(internalEventThreadMutex);
            if (event->type == DRM_XE_EUDEBUG_EVENT_EU_ATTENTION) {
                newestAttSeqNo.store(event->seqno);
            }

            auto memory = std::make_unique<uint64_t[]>(maxEventSize / sizeof(uint64_t));
            memcpy(memory.get(), event, maxEventSize);

            internalEventQueue.push(std::move(memory));
            internalEventCondition.notify_one();
        }
    }
}

int DebugSessionLinuxXe::ioctl(unsigned long request, void *arg) {
    return ioctlHandler->ioctl(fd, request, arg);
}

ze_result_t DebugSessionLinuxXe::readEventImp(drm_xe_eudebug_event *drmDebugEvent) {
    auto ret = ioctl(DRM_XE_EUDEBUG_IOCTL_READ_EVENT, drmDebugEvent);
    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT failed: retCode: %d errno = %d\n", ret, errno);
        return ZE_RESULT_NOT_READY;
    } else if (drmDebugEvent->flags & ~static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_CREATE | DRM_XE_EUDEBUG_EVENT_DESTROY | DRM_XE_EUDEBUG_EVENT_STATE_CHANGE | DRM_XE_EUDEBUG_EVENT_NEED_ACK)) {
        PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT unsupported flag = %d\n", (int)drmDebugEvent->flags);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

void DebugSessionLinuxXe::handleEvent(drm_xe_eudebug_event *event) {
    auto type = event->type;

    PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type = %u flags = %u seqno = %llu len = %lu",
                            (uint16_t)event->type, (uint16_t)event->flags, (uint64_t)event->seqno, (uint32_t)event->len);

    switch (type) {
    case DRM_XE_EUDEBUG_EVENT_OPEN: {
        auto clientEvent = reinterpret_cast<drm_xe_eudebug_event_client *>(event);

        if (event->flags & DRM_XE_EUDEBUG_EVENT_CREATE) {
            DEBUG_BREAK_IF(clientHandleToConnection.find(clientEvent->client_handle) != clientHandleToConnection.end());
            clientHandleToConnection[clientEvent->client_handle].reset(new ClientConnectionXe);
            clientHandleToConnection[clientEvent->client_handle]->client = *clientEvent;
            clientHandle = clientEvent->client_handle;
        }

        if (event->flags & DRM_XE_EUDEBUG_EVENT_DESTROY) {
            clientHandleClosed = clientEvent->client_handle;
        }

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_OPEN client.handle = %llu\n",
                                (uint64_t)clientEvent->client_handle);

    } break;

    case DRM_XE_EUDEBUG_EVENT_VM: {
        drm_xe_eudebug_event_vm *vm = reinterpret_cast<drm_xe_eudebug_event_vm *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM client_handle = %llu vm_handle = %llu\n",
                                (uint64_t)vm->client_handle, (uint64_t)vm->vm_handle);

        if (event->flags & DRM_XE_EUDEBUG_EVENT_CREATE) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->client_handle]->vmIds.emplace(static_cast<uint64_t>(vm->vm_handle));
        }

        if (event->flags & DRM_XE_EUDEBUG_EVENT_DESTROY) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->client_handle]->vmIds.erase(static_cast<uint64_t>(vm->vm_handle));
        }
    } break;

    case DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE: {
        drm_xe_eudebug_event_exec_queue *execQueue = reinterpret_cast<drm_xe_eudebug_event_exec_queue *>(event);

        if (event->flags & DRM_XE_EUDEBUG_EVENT_CREATE) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(execQueue->client_handle) == clientHandleToConnection.end());

            if (!processEntryEventGenerated) {
                zet_debug_event_t debugEvent = {};
                debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;
                pushApiEvent(debugEvent);
                processEntryEventGenerated = true;
            }
            std::lock_guard<std::mutex> lock(asyncThreadMutex);
            clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].vmHandle = execQueue->vm_handle;
            clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].engineClass = execQueue->engine_class;
            for (uint16_t idx = 0; idx < execQueue->width; idx++) {
                uint64_t lrcHandle = execQueue->lrc_handle[idx];
                clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].lrcHandles.push_back(lrcHandle);
                clientHandleToConnection[execQueue->client_handle]->lrcHandleToVmHandle[lrcHandle] = execQueue->vm_handle;
            }
        }

        if (event->flags & DRM_XE_EUDEBUG_EVENT_DESTROY) {
            {
                std::lock_guard<std::mutex> lock(asyncThreadMutex);
                for (uint16_t idx = 0; idx < execQueue->width; idx++) {
                    clientHandleToConnection[execQueue->client_handle]->lrcHandleToVmHandle.erase(execQueue->lrc_handle[idx]);
                }
                clientHandleToConnection[execQueue->client_handle]->execQueues.erase(execQueue->exec_queue_handle);
            }
            {
                bool lastExecQueue = true;
                for (const auto &connection : clientHandleToConnection) {
                    if (!connection.second->execQueues.empty()) {
                        lastExecQueue = false;
                        break;
                    }
                }
                if (lastExecQueue) {
                    zet_debug_event_t debugEvent = {};
                    debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT;
                    pushApiEvent(debugEvent);
                    processEntryEventGenerated = false;
                }
            }
        }

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE client_handle = %llu vm_handle = %llu exec_queue_handle = %llu engine_class = %u\n",
                                (uint64_t)execQueue->client_handle, (uint64_t)execQueue->vm_handle,
                                (uint64_t)execQueue->exec_queue_handle, (uint16_t)execQueue->engine_class);
    } break;

    case DRM_XE_EUDEBUG_EVENT_EU_ATTENTION: {
        drm_xe_eudebug_event_eu_attention *attention = reinterpret_cast<drm_xe_eudebug_event_eu_attention *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_EU_ATTENTION client_handle = %llu flags = %llu bitmask_size = %lu exec_queue_handle = %llu lrc_handle = %llu\n",
                                (uint64_t)attention->client_handle, (uint64_t)attention->flags,
                                (uint32_t)attention->bitmask_size, uint64_t(attention->exec_queue_handle), uint64_t(attention->lrc_handle));
        if (attention->base.seqno < newestAttSeqNo.load()) {
            PRINT_DEBUGGER_INFO_LOG("Dropping stale ATT event seqno=%llu\n", (uint64_t)attention->base.seqno);
        } else {
            handleAttentionEvent(attention);
        }
    } break;

    case DRM_XE_EUDEBUG_EVENT_VM_BIND: {
        drm_xe_eudebug_event_vm_bind *vmBind = reinterpret_cast<drm_xe_eudebug_event_vm_bind *>(event);
        UNRECOVERABLE_IF(clientHandleToConnection.find(vmBind->client_handle) == clientHandleToConnection.end());

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM_BIND client_handle = %llu vm_handle = %llu num_binds = %llu vmBindflag=%lu\n",
                                static_cast<uint64_t>(vmBind->client_handle), static_cast<uint64_t>(vmBind->vm_handle),
                                static_cast<uint64_t>(vmBind->num_binds), static_cast<uint32_t>(vmBind->flags));

        auto &connection = clientHandleToConnection[vmBind->client_handle];
        UNRECOVERABLE_IF(connection->vmBindMap.find(vmBind->base.seqno) != connection->vmBindMap.end());
        auto &vmBindData = connection->vmBindMap[vmBind->base.seqno];
        vmBindData.vmBind = *vmBind;
        vmBindData.pendingNumBinds = vmBind->num_binds;
    } break;

    case DRM_XE_EUDEBUG_EVENT_VM_BIND_OP: {
        drm_xe_eudebug_event_vm_bind_op *vmBindOp = reinterpret_cast<drm_xe_eudebug_event_vm_bind_op *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: drm_xe_eudebug_event_vm_bind_op vm_bind_ref_seqno = %llu num_extensions = %llu addr = 0x%llx range = %llu\n",
                                static_cast<uint64_t>(vmBindOp->vm_bind_ref_seqno), static_cast<uint64_t>(vmBindOp->num_extensions),
                                static_cast<uint64_t>(vmBindOp->addr), static_cast<uint64_t>(vmBindOp->range));

        auto &vmBindMap = clientHandleToConnection[clientHandle]->vmBindMap;
        UNRECOVERABLE_IF(vmBindMap.find(vmBindOp->vm_bind_ref_seqno) == vmBindMap.end());
        auto &vmBindData = vmBindMap[vmBindOp->vm_bind_ref_seqno];
        UNRECOVERABLE_IF(!vmBindData.pendingNumBinds);

        auto &vmBindOpData = vmBindData.vmBindOpMap[vmBindOp->base.seqno];
        vmBindOpData.pendingNumExtensions = vmBindOp->num_extensions;
        vmBindOpData.vmBindOp = *vmBindOp;
        vmBindData.pendingNumBinds--;
        clientHandleToConnection[clientHandle]->vmBindIdentifierMap[vmBindOp->base.seqno] = vmBindOp->vm_bind_ref_seqno;
        handleVmBind(vmBindData);
    } break;

    case DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE: {
        drm_xe_eudebug_event_vm_bind_ufence *vmBindUfence = reinterpret_cast<drm_xe_eudebug_event_vm_bind_ufence *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE vm_bind_ref_seqno = %llu\n",
                                static_cast<uint64_t>(vmBindUfence->vm_bind_ref_seqno));

        auto &vmBindMap = clientHandleToConnection[clientHandle]->vmBindMap;
        UNRECOVERABLE_IF(vmBindMap.find(vmBindUfence->vm_bind_ref_seqno) == vmBindMap.end());
        uint32_t uFenceRequired = vmBindMap[vmBindUfence->vm_bind_ref_seqno].vmBind.flags & DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
        UNRECOVERABLE_IF(!uFenceRequired);
        UNRECOVERABLE_IF(vmBindMap[vmBindUfence->vm_bind_ref_seqno].uFenceReceived); // Dont expect multiple UFENCE for same vm_bind
        vmBindMap[vmBindUfence->vm_bind_ref_seqno].uFenceReceived = true;
        vmBindMap[vmBindUfence->vm_bind_ref_seqno].vmBindUfence = *vmBindUfence;
        handleVmBind(vmBindMap[vmBindUfence->vm_bind_ref_seqno]);
    } break;

    case DRM_XE_EUDEBUG_EVENT_METADATA: {
        drm_xe_eudebug_event_metadata *metaData = reinterpret_cast<drm_xe_eudebug_event_metadata *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_METADATA client_handle = %llu metadata_handle = %llu type = %llu len = %llu\n",
                                (uint64_t)metaData->client_handle, (uint64_t)metaData->metadata_handle, (uint64_t)metaData->type, (uint64_t)metaData->len);

        handleMetadataEvent(metaData);
    } break;

    case DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA: {
        drm_xe_eudebug_event_vm_bind_op_metadata *vmBindOpMetadata = reinterpret_cast<drm_xe_eudebug_event_vm_bind_op_metadata *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA vm_bind_op_ref_seqno = %llu metadata_handle = %llu metadata_cookie = %llu\n",
                                static_cast<uint64_t>(vmBindOpMetadata->vm_bind_op_ref_seqno), static_cast<uint64_t>(vmBindOpMetadata->metadata_handle),
                                static_cast<uint64_t>(vmBindOpMetadata->metadata_cookie));

        auto &vmBindMap = clientHandleToConnection[clientHandle]->vmBindMap;
        auto &vmBindIdentifierMap = clientHandleToConnection[clientHandle]->vmBindIdentifierMap;
        UNRECOVERABLE_IF(vmBindIdentifierMap.find(vmBindOpMetadata->vm_bind_op_ref_seqno) == vmBindIdentifierMap.end());
        VmBindSeqNo vmBindSeqNo = vmBindIdentifierMap[vmBindOpMetadata->vm_bind_op_ref_seqno];
        UNRECOVERABLE_IF(vmBindMap.find(vmBindSeqNo) == vmBindMap.end());
        auto &vmBindOpData = vmBindMap[vmBindSeqNo].vmBindOpMap[vmBindOpMetadata->vm_bind_op_ref_seqno];
        UNRECOVERABLE_IF(!vmBindOpData.pendingNumExtensions);
        vmBindOpData.vmBindOpMetadataVec.push_back(*vmBindOpMetadata);
        vmBindOpData.pendingNumExtensions--;
        handleVmBind(vmBindMap[vmBindSeqNo]);
    } break;

    default:
        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: UNHANDLED %u flags = %u len = %lu\n", (uint16_t)event->type, (uint16_t)event->flags, (uint32_t)event->len);
        break;
    }
}

bool DebugSessionLinuxXe::canHandleVmBind(VmBindData &vmBindData) const {
    if (vmBindData.pendingNumBinds) {
        return false;
    }
    for (const auto &vmBindOpData : vmBindData.vmBindOpMap) {
        if (vmBindOpData.second.pendingNumExtensions) {
            return false;
        }
    }
    uint32_t uFenceRequired = vmBindData.vmBind.flags & DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    if (uFenceRequired) {
        if (!vmBindData.uFenceReceived) {
            return false;
        }
    }

    return true;
}

void DebugSessionLinuxXe::handleVmBind(VmBindData &vmBindData) {
    if (!canHandleVmBind(vmBindData)) {
        return;
    }
    bool shouldAckEvent = vmBindData.vmBind.flags & DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE;
    auto connection = clientHandleToConnection[clientHandle].get();
    auto elfHandleInVmBind = invalidHandle;

    uint64_t moduleHandle = invalidHandle;
    uint64_t isaAddr = 0;
    bool triggerModuleLoadEvent = false;

    for (auto &vmBindOpData : vmBindData.vmBindOpMap) {
        auto &vmBindOp = vmBindOpData.second.vmBindOp;
        for (const auto &vmBindOpMetadata : vmBindOpData.second.vmBindOpMetadataVec) {
            auto &metaDataEntry = connection->metaDataMap[vmBindOpMetadata.metadata_handle];
            if (vmBindOp.base.flags & DRM_XE_EUDEBUG_EVENT_CREATE) {
                {
                    std::lock_guard<std::mutex> lock(asyncThreadMutex);
                    if (metaDataEntry.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA) {
                        connection->vmToStateBaseAreaBindInfo[vmBindData.vmBind.vm_handle] = {vmBindOp.addr, vmBindOp.range};
                    }
                    if (metaDataEntry.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA) {
                        connection->vmToContextStateSaveAreaBindInfo[vmBindData.vmBind.vm_handle] = {vmBindOp.addr, vmBindOp.range};
                    }
                    if (metaDataEntry.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA) {
                        connection->vmToModuleDebugAreaBindInfo[vmBindData.vmBind.vm_handle] = {vmBindOp.addr, vmBindOp.range};
                    }
                }

                if (metaDataEntry.metadata.type == DRM_XE_DEBUG_METADATA_ELF_BINARY) {
                    isaAddr = vmBindOp.addr;
                    if (connection->isaMap[0].find(vmBindOp.addr) == connection->isaMap[0].end()) {
                        auto &isaMap = connection->isaMap[0];
                        auto &elfMap = connection->elfMap;
                        auto isa = std::make_unique<IsaAllocation>();
                        isa->bindInfo = {vmBindOp.addr, vmBindOp.range};
                        isa->vmHandle = vmBindData.vmBind.vm_handle;
                        isa->elfHandle = vmBindOpMetadata.metadata_handle;
                        isa->moduleBegin = reinterpret_cast<uint64_t>(metaDataEntry.data.get());
                        isa->moduleEnd = isa->moduleBegin + metaDataEntry.metadata.len;
                        isa->tileInstanced = false;
                        isa->validVMs.insert(vmBindData.vmBind.vm_handle);
                        isa->perKernelModule = false;
                        elfMap[isa->moduleBegin] = isa->elfHandle;
                        isaMap[vmBindOp.addr] = std::move(isa);
                        isaMap[vmBindOp.addr]->moduleLoadEventAck = true;
                        elfHandleInVmBind = vmBindOpMetadata.metadata_handle;
                    } else {
                        auto &isa = connection->isaMap[0][vmBindOp.addr];
                        isa->validVMs.insert(vmBindData.vmBind.vm_handle);
                    }
                }
                if (metaDataEntry.metadata.type == DRM_XE_DEBUG_METADATA_PROGRAM_MODULE) {
                    auto &module = connection->metaDataToModule[vmBindOpMetadata.metadata_handle];
                    module.segmentVmBindCounter[0]++;
                    DEBUG_BREAK_IF(module.loadAddresses[0].size() > module.segmentCount);

                    bool canTriggerEvent = module.loadAddresses[0].size() == (module.segmentCount - 1);
                    module.loadAddresses[0].insert(vmBindOp.addr);
                    moduleHandle = vmBindOpMetadata.metadata_handle;
                    if (canTriggerEvent && module.loadAddresses[0].size() == module.segmentCount) {
                        triggerModuleLoadEvent = true;
                    }
                }
            }
        }

        if (vmBindOp.base.flags & DRM_XE_EUDEBUG_EVENT_DESTROY) {
            if (connection->isaMap[0].count(vmBindOp.addr)) {
                auto &isa = connection->isaMap[0][vmBindOp.addr];
                if (isa->validVMs.count(vmBindData.vmBind.vm_handle)) {
                    auto &module = connection->metaDataToModule[isa->moduleHandle];
                    module.segmentVmBindCounter[0]--;
                    if (module.segmentVmBindCounter[0] == 0) {
                        zet_debug_event_t debugEvent = {};
                        auto &metaDataEntry = connection->metaDataMap[isa->moduleHandle];
                        auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                        auto loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[0].begin(), module.loadAddresses[0].end()));
                        debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD;
                        debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                        debugEvent.info.module.load = loadAddress;
                        auto &elfMetadata = connection->metaDataMap[isa->elfHandle];
                        debugEvent.info.module.moduleBegin = reinterpret_cast<uint64_t>(elfMetadata.data.get());
                        debugEvent.info.module.moduleEnd = reinterpret_cast<uint64_t>(elfMetadata.data.get()) + elfMetadata.metadata.len;
                        pushApiEvent(debugEvent, metaDataEntry.metadata.metadata_handle);
                        module.loadAddresses[0].clear();
                        module.moduleLoadEventAcked[0] = false;
                    }
                    isa->validVMs.erase(vmBindData.vmBind.vm_handle);
                }
            }
        }
    }

    if (isaAddr && moduleHandle != invalidHandle) {
        connection->isaMap[0][isaAddr]->moduleHandle = moduleHandle;
    }

    if (triggerModuleLoadEvent) {
        DEBUG_BREAK_IF(moduleHandle == invalidHandle);
        DEBUG_BREAK_IF(elfHandleInVmBind == invalidHandle);
        auto &metaDataEntry = connection->metaDataMap[moduleHandle];
        auto &module = connection->metaDataToModule[moduleHandle];
        auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
        auto loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[0].begin(), module.loadAddresses[0].end()));
        PRINT_DEBUGGER_INFO_LOG("Zebin module loaded at: %p, with %u isa allocations", (void *)loadAddress, module.segmentCount);

        auto &elfMetadata = connection->metaDataMap[elfHandleInVmBind];
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
        debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
        debugEvent.info.module.load = loadAddress;
        debugEvent.info.module.moduleBegin = reinterpret_cast<uint64_t>(elfMetadata.data.get());
        debugEvent.info.module.moduleEnd = reinterpret_cast<uint64_t>(elfMetadata.data.get()) + elfMetadata.metadata.len;
        debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;

        pushApiEvent(debugEvent, metaDataEntry.metadata.metadata_handle);
        {
            std::lock_guard<std::mutex> lock(asyncThreadMutex);
            if (vmBindData.vmBind.flags & DRM_XE_EUDEBUG_EVENT_VM_BIND_FLAG_UFENCE) {
                if (vmBindData.vmBindUfence.base.flags & DRM_XE_EUDEBUG_EVENT_NEED_ACK) {
                    EventToAck ackEvent(vmBindData.vmBindUfence.base.seqno, vmBindData.vmBindUfence.base.type);
                    module.ackEvents[0].push_back(ackEvent);
                    shouldAckEvent = false;
                }
            }
        }
    }

    if (shouldAckEvent && (vmBindData.vmBindUfence.base.flags & DRM_XE_EUDEBUG_EVENT_NEED_ACK)) {
        EventToAck ackEvent(vmBindData.vmBindUfence.base.seqno, vmBindData.vmBindUfence.base.type);
        eventAckIoctl(ackEvent);
    }
}

void DebugSessionLinuxXe::handleMetadataEvent(drm_xe_eudebug_event_metadata *metaData) {
    bool destroy = metaData->base.flags & DRM_XE_EUDEBUG_EVENT_DESTROY;
    bool create = metaData->base.flags & DRM_XE_EUDEBUG_EVENT_CREATE;

    UNRECOVERABLE_IF(clientHandleToConnection.find(metaData->client_handle) == clientHandleToConnection.end());
    const auto &connection = clientHandleToConnection[metaData->client_handle];

    if (destroy && connection->metaDataMap[metaData->metadata_handle].metadata.type == DRM_XE_DEBUG_METADATA_PROGRAM_MODULE) {
        DEBUG_BREAK_IF(connection->metaDataToModule[metaData->metadata_handle].segmentVmBindCounter[0] != 0 ||
                       connection->metaDataToModule[metaData->metadata_handle].loadAddresses[0].size() > 0);

        connection->metaDataToModule.erase(metaData->metadata_handle);
    }

    if (create) {
        if (!metaData->len) {
            connection->metaDataMap[metaData->metadata_handle].metadata = *metaData;
            return;
        }

        drm_xe_eudebug_read_metadata readMetadata{};
        auto ptr = std::make_unique<char[]>(metaData->len);
        readMetadata.client_handle = metaData->client_handle;
        readMetadata.metadata_handle = static_cast<decltype(readMetadata.metadata_handle)>(metaData->metadata_handle);
        readMetadata.ptr = reinterpret_cast<uint64_t>(ptr.get());
        readMetadata.size = metaData->len;
        auto ret = ioctl(DRM_XE_EUDEBUG_IOCTL_READ_METADATA, &readMetadata);
        if (ret != 0) {
            PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_METADATA ret = %d errno = %d\n", ret, errno);
            return;
        }

        connection->metaDataMap[metaData->metadata_handle].metadata = *metaData;
        connection->metaDataMap[metaData->metadata_handle].data = std::move(ptr);

        if (metaData->type == DRM_XE_DEBUG_METADATA_PROGRAM_MODULE) {
            auto handle = metaData->metadata_handle;
            auto &newModule = connection->metaDataToModule[handle];
            newModule.segmentCount = 0;
            newModule.moduleHandle = handle;
            for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                newModule.segmentVmBindCounter[i] = 0;
                newModule.loadAddresses[i].clear();
                newModule.moduleLoadEventAcked[i] = false;
            }
        }
        extractMetaData(metaData->client_handle, connection->metaDataMap[metaData->metadata_handle]);
    }
}

void DebugSessionLinuxXe::extractMetaData(uint64_t client, const MetaData &metaData) {
    if (metaData.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA ||
        metaData.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA ||
        metaData.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA) {
        UNRECOVERABLE_IF(metaData.metadata.len != 8);
        uint64_t *data = (uint64_t *)metaData.data.get();

        if (metaData.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA) {
            clientHandleToConnection[client]->stateBaseAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("SbaTrackingBuffer GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->stateBaseAreaGpuVa);
        }
        if (metaData.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA) {
            clientHandleToConnection[client]->moduleDebugAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ModuleHeapDebugArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->moduleDebugAreaGpuVa);
        }
        if (metaData.metadata.type == WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA) {
            clientHandleToConnection[client]->contextStateSaveAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ContextSaveArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->contextStateSaveAreaGpuVa);
        }
    }

    if (metaData.metadata.type == DRM_XE_DEBUG_METADATA_PROGRAM_MODULE) {
        uint32_t segmentCount = 0;
        memcpy_s(&segmentCount, sizeof(uint32_t), metaData.data.get(), metaData.metadata.len);
        clientHandleToConnection[client]->metaDataToModule[metaData.metadata.metadata_handle].segmentCount = segmentCount;
        PRINT_DEBUGGER_INFO_LOG("Zebin module = %ull, segment count = %ul", metaData.metadata.metadata_handle, segmentCount);
    }
}

int DebugSessionLinuxXe::openVmFd(uint64_t vmHandle, [[maybe_unused]] bool readOnly) {
    drm_xe_eudebug_vm_open vmOpen = {
        .extensions = 0,
        .client_handle = clientHandle,
        .vm_handle = vmHandle,
        .flags = 0,
        .timeout_ns = 5000000000u};

    return ioctl(DRM_XE_EUDEBUG_IOCTL_VM_OPEN, &vmOpen);
}

int DebugSessionLinuxXe::flushVmCache(int vmfd) {
    int retVal = ioctlHandler->fsync(vmfd);
    if (retVal != 0) {
        PRINT_DEBUGGER_ERROR_LOG("Failed to fsync VM fd=%d errno=%d\n", vmfd, errno);
    }
    return retVal;
}

uint64_t DebugSessionLinuxXe::getVmHandleFromClientAndlrcHandle(uint64_t clientHandle, uint64_t lrcHandle) {

    if (clientHandleToConnection.find(clientHandle) == clientHandleToConnection.end()) {
        return invalidHandle;
    }

    auto &clientConnection = clientHandleToConnection[clientHandle];
    if (clientConnection->lrcHandleToVmHandle.find(lrcHandle) == clientConnection->lrcHandleToVmHandle.end()) {
        return invalidHandle;
    }

    return clientConnection->lrcHandleToVmHandle[lrcHandle];
}

void DebugSessionLinuxXe::handleAttentionEvent(drm_xe_eudebug_event_eu_attention *attention) {
    if (interruptSent) {
        if (attention->base.seqno <= euControlInterruptSeqno) {
            PRINT_DEBUGGER_INFO_LOG("Discarding EU ATTENTION event for interrupt request. Event seqno == %llu <= %llu == interrupt seqno\n",
                                    static_cast<uint64_t>(attention->base.seqno), euControlInterruptSeqno);
            return;
        }
    }

    newAttentionRaised();
    std::vector<EuThread::ThreadId> threadsWithAttention;
    AttentionEventFields attentionEventFields;
    attentionEventFields.bitmask = attention->bitmask;
    attentionEventFields.bitmaskSize = attention->bitmask_size;
    attentionEventFields.clientHandle = attention->client_handle;
    attentionEventFields.contextHandle = attention->exec_queue_handle;
    attentionEventFields.lrcHandle = attention->lrc_handle;

    return updateStoppedThreadsAndCheckTriggerEvents(attentionEventFields, 0, threadsWithAttention);
}

int DebugSessionLinuxXe::threadControlInterruptAll(drm_xe_eudebug_eu_control &euControl) {
    int euControlRetVal = -1;

    DEBUG_BREAK_IF(clientHandleToConnection.find(clientHandle) == clientHandleToConnection.end());
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    for (const auto &execQueue : clientHandleToConnection[clientHandle]->execQueues) {
        euControl.exec_queue_handle = execQueue.first;
        for (const auto &lrcHandle : execQueue.second.lrcHandles) {
            euControl.lrc_handle = lrcHandle;
            euControlRetVal = ioctl(DRM_XE_EUDEBUG_IOCTL_EU_CONTROL, &euControl);
            if (euControlRetVal != 0) {
                PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d, execQueueHandle = %llu lrcHandle = %llu\n",
                                         euControlRetVal, errno, static_cast<uint32_t>(euControl.cmd), static_cast<uint64_t>(euControl.exec_queue_handle),
                                         static_cast<uint64_t>(euControl.lrc_handle));
            } else {
                DEBUG_BREAK_IF(euControlInterruptSeqno >= euControl.seqno);
                euControlInterruptSeqno = euControl.seqno;
                PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL: seqno = %llu command = %u\n", static_cast<uint64_t>(euControl.seqno),
                                        static_cast<uint32_t>(euControl.cmd));
            }
        }
    }

    return euControlRetVal;
}

int DebugSessionLinuxXe::threadControlStopped(drm_xe_eudebug_eu_control &euControl, std::unique_ptr<uint8_t[]> &bitmaskOut, size_t &bitmaskSizeOut) {
    int euControlRetVal = -1;
    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({}, hwInfo, bitmask, bitmaskSize);
    euControl.bitmask_size = static_cast<uint32_t>(bitmaskSize);
    euControl.bitmask_ptr = reinterpret_cast<uint64_t>(bitmask.get());

    DEBUG_BREAK_IF(clientHandleToConnection.find(clientHandle) == clientHandleToConnection.end());
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    for (const auto &execQueue : clientHandleToConnection[clientHandle]->execQueues) {
        euControl.exec_queue_handle = execQueue.first;
        for (const auto &lrcHandle : execQueue.second.lrcHandles) {
            euControl.lrc_handle = lrcHandle;
            euControlRetVal = ioctl(DRM_XE_EUDEBUG_IOCTL_EU_CONTROL, &euControl);
            if (euControlRetVal != 0) {
                PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d, execQueueHandle = %llu lrcHandle = %llu\n",
                                         euControlRetVal, errno, static_cast<uint32_t>(euControl.cmd), static_cast<uint64_t>(euControl.exec_queue_handle),
                                         static_cast<uint64_t>(euControl.lrc_handle));
            } else {
                PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL: seqno = %llu command = %u\n", static_cast<uint64_t>(euControl.seqno),
                                        static_cast<uint32_t>(euControl.cmd));
                break;
            }
        }
        if (euControlRetVal == 0) {
            break;
        }
    }

    printBitmask(bitmask.get(), bitmaskSize);
    bitmaskOut = std::move(bitmask);
    UNRECOVERABLE_IF(bitmaskOut.get() == nullptr);
    bitmaskSizeOut = euControl.bitmask_size;
    return euControlRetVal;
}

int DebugSessionLinuxXe::threadControlResume(const std::vector<EuThread::ThreadId> &threads, drm_xe_eudebug_eu_control &euControl) {
    int euControlRetVal = -1;
    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);
    euControl.bitmask_size = static_cast<uint32_t>(bitmaskSize);
    euControl.bitmask_ptr = reinterpret_cast<uint64_t>(bitmask.get());
    printBitmask(bitmask.get(), bitmaskSize);
    uint64_t execQueueHandle{0};
    uint64_t lrcHandle{0};
    allThreads[threads[0]]->getContextHandle(execQueueHandle);
    allThreads[threads[0]]->getLrcHandle(lrcHandle);
    euControl.exec_queue_handle = execQueueHandle;
    euControl.lrc_handle = lrcHandle;

    euControlRetVal = ioctl(DRM_XE_EUDEBUG_IOCTL_EU_CONTROL, &euControl);
    if (euControlRetVal != 0) {
        PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d, execQueueHandle = %llu lrcHandle = %llu\n",
                                 euControlRetVal, errno, static_cast<uint32_t>(euControl.cmd), static_cast<uint64_t>(euControl.exec_queue_handle),
                                 static_cast<uint64_t>(euControl.lrc_handle));
    } else {
        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL: seqno = %llu command = %u\n", static_cast<uint64_t>(euControl.seqno), static_cast<uint32_t>(euControl.cmd));
    }

    return euControlRetVal;
}

int DebugSessionLinuxXe::threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile,
                                       ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmaskOut, size_t &bitmaskSizeOut) {

    bitmaskSizeOut = 0;
    struct drm_xe_eudebug_eu_control euControl = {};
    euControl.client_handle = clientHandle;
    euControl.bitmask_size = 0;
    euControl.bitmask_ptr = 0;

    switch (threadCmd) {
    case ThreadControlCmd::interruptAll:
        euControl.cmd = DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL;
        return threadControlInterruptAll(euControl);
    case ThreadControlCmd::resume:
        euControl.cmd = DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME;
        return threadControlResume(threads, euControl);
    case ThreadControlCmd::stopped:
        euControl.cmd = DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED;
        return threadControlStopped(euControl, bitmaskOut, bitmaskSizeOut);
    default:
        break;
    }
    return -1;
}

void DebugSessionLinuxXe::updateContextAndLrcHandlesForThreadsWithAttention(EuThread::ThreadId threadId, AttentionEventFields &attention) {
    allThreads[threadId]->setContextHandle(attention.contextHandle);
    allThreads[threadId]->setLrcHandle(attention.lrcHandle);
}

int DebugSessionLinuxXe::eventAckIoctl(EventToAck &event) {
    drm_xe_eudebug_ack_event eventToAck = {};
    eventToAck.type = event.type;
    eventToAck.seqno = event.seqno;
    eventToAck.flags = 0;
    auto ret = ioctl(DRM_XE_EUDEBUG_IOCTL_ACK_EVENT, &eventToAck);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_ACK_EVENT seqno = %llu ret = %d errno = %d\n", static_cast<uint64_t>(eventToAck.seqno), ret, ret != 0 ? errno : 0);
    return ret;
}

} // namespace L0