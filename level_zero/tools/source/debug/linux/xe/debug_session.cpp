/*
 * Copyright (C) 2023-2025 Intel Corporation
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
    auto sysFsPciPath = DrmHelper::getSysFsPciPath(device);
    euDebugInterface = NEO::EuDebugInterface::create(sysFsPciPath);
    if (euDebugInterface) {
        ioctlHandler.reset(new IoctlHandlerXe(*euDebugInterface));
        if (params) {
            this->xeDebuggerVersion = reinterpret_cast<NEO::EuDebugConnect *>(params)->version;
        }
    }
};
DebugSessionLinuxXe::~DebugSessionLinuxXe() {

    closeAsyncThread();
    closeInternalEventsThread();
    closeFd();
}

DebugSession *DebugSessionLinuxXe::createLinuxSession(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {

    NEO::EuDebugConnect open = {
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

    auto debugEvent = reinterpret_cast<NEO::EuDebugEvent *>(eventMemory.get());
    handleEvent(debugEvent);
    if (debugEvent->type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeExecQueuePlacements) ||
        debugEvent->type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVmBindOp) ||
        debugEvent->type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVmBindUfence) ||
        debugEvent->type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVmBindOpMetadata)) {
        processPendingVmBindEvents();
    }
    return true;
}

void *DebugSessionLinuxXe::asyncThreadFunction(void *arg) {
    DebugSessionLinuxXe *self = reinterpret_cast<DebugSessionLinuxXe *>(arg);
    if (NEO::debugManager.flags.DebugUmdFifoPollInterval.get() != -1) {
        self->fifoPollInterval = NEO::debugManager.flags.DebugUmdFifoPollInterval.get();
    }
    if (NEO::debugManager.flags.DebugUmdInterruptTimeout.get() != -1) {
        self->interruptTimeout = NEO::debugManager.flags.DebugUmdInterruptTimeout.get();
    }
    if (NEO::debugManager.flags.DebugUmdMaxReadWriteRetry.get() != -1) {
        self->maxRetries = NEO::debugManager.flags.DebugUmdMaxReadWriteRetry.get();
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread start\n", "");

    while (self->asyncThread.threadActive) {
        self->handleEventsAsync();
        self->pollFifo();
        self->generateEventsAndResumeStoppedThreads();
        self->sendInterrupts();
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread closing\n", "");

    return nullptr;
}

void DebugSessionLinuxXe::startAsyncThread() {
    asyncThread.thread = NEO::Thread::createFunc(asyncThreadFunction, reinterpret_cast<void *>(this));
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

        uint8_t maxEventBuffer[sizeof(NEO::EuDebugEvent) + maxEventSize];
        auto event = reinterpret_cast<NEO::EuDebugEvent *>(maxEventBuffer);
        event->len = maxEventSize;
        event->type = euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeRead);
        event->flags = 0;

        result = readEventImp(event);

        if (result == ZE_RESULT_SUCCESS) {
            std::lock_guard<std::mutex> lock(internalEventThreadMutex);
            if (eventTypeIsAttention(event->type)) {
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

ze_result_t DebugSessionLinuxXe::readEventImp(NEO::EuDebugEvent *drmDebugEvent) {
    auto ret = ioctl(euDebugInterface->getParamValue(NEO::EuDebugParam::ioctlReadEvent), drmDebugEvent);
    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT failed: retCode: %d errno = %d\n", ret, errno);
        return ZE_RESULT_NOT_READY;
    } else if (drmDebugEvent->flags & ~static_cast<uint32_t>(
                                          euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitCreate) |
                                          euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitDestroy) |
                                          euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitStateChange) |
                                          euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitNeedAck))) {
        PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT unsupported flag = %d\n", (int)drmDebugEvent->flags);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

void DebugSessionLinuxXe::handleEvent(NEO::EuDebugEvent *event) {
    auto type = event->type;

    PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type = %u flags = %u seqno = %llu len = %lu",
                            (uint16_t)event->type, (uint16_t)event->flags, (uint64_t)event->seqno, (uint32_t)event->len);

    if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeOpen)) {
        auto clientEvent = reinterpret_cast<NEO::EuDebugEventClient *>(event);

        if (event->flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitCreate)) {
            DEBUG_BREAK_IF(clientHandleToConnection.find(clientEvent->clientHandle) != clientHandleToConnection.end());
            clientHandleToConnection[clientEvent->clientHandle].reset(new ClientConnectionXe);
            clientHandleToConnection[clientEvent->clientHandle]->client = *clientEvent;
        }

        if (event->flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitDestroy)) {
            clientHandleClosed = clientEvent->clientHandle;
        }

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_OPEN client.handle = %llu\n",
                                (uint64_t)clientEvent->clientHandle);

    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVm)) {
        NEO::EuDebugEventVm *vm = reinterpret_cast<NEO::EuDebugEventVm *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM client_handle = %llu vm_handle = %llu\n",
                                (uint64_t)vm->clientHandle, (uint64_t)vm->vmHandle);

        if (event->flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitCreate)) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->clientHandle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->clientHandle]->vmIds.emplace(static_cast<uint64_t>(vm->vmHandle));
        }

        if (event->flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitDestroy)) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->clientHandle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->clientHandle]->vmIds.erase(static_cast<uint64_t>(vm->vmHandle));
        }
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeExecQueue)) {
        NEO::EuDebugEventExecQueue *execQueue = reinterpret_cast<NEO::EuDebugEventExecQueue *>(event);

        if (event->flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitCreate)) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(execQueue->clientHandle) == clientHandleToConnection.end());

            if (!processEntryEventGenerated) {
                zet_debug_event_t debugEvent = {};
                debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;
                pushApiEvent(debugEvent);
                processEntryEventGenerated = true;
            }
            std::lock_guard<std::mutex> lock(asyncThreadMutex);
            clientHandleToConnection[execQueue->clientHandle]->execQueues[execQueue->execQueueHandle].vmHandle = execQueue->vmHandle;
            clientHandleToConnection[execQueue->clientHandle]->execQueues[execQueue->execQueueHandle].engineClass = execQueue->engineClass;
            for (uint16_t idx = 0; idx < execQueue->width; idx++) {
                uint64_t lrcHandle = execQueue->lrcHandle[idx];
                clientHandleToConnection[execQueue->clientHandle]->execQueues[execQueue->execQueueHandle].lrcHandles.push_back(lrcHandle);
                clientHandleToConnection[execQueue->clientHandle]->lrcHandleToVmHandle[lrcHandle] = execQueue->vmHandle;
            }
        }

        if (event->flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitDestroy)) {
            {
                std::lock_guard<std::mutex> lock(asyncThreadMutex);
                for (uint16_t idx = 0; idx < execQueue->width; idx++) {
                    clientHandleToConnection[execQueue->clientHandle]->lrcHandleToVmHandle.erase(execQueue->lrcHandle[idx]);
                }
                clientHandleToConnection[execQueue->clientHandle]->execQueues.erase(execQueue->execQueueHandle);
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
                                (uint64_t)execQueue->clientHandle, (uint64_t)execQueue->vmHandle,
                                (uint64_t)execQueue->execQueueHandle, (uint16_t)execQueue->engineClass);
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeEuAttention)) {
        NEO::EuDebugEventEuAttention *attention = reinterpret_cast<NEO::EuDebugEventEuAttention *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_EU_ATTENTION client_handle = %llu flags = %llu bitmask_size = %lu exec_queue_handle = %llu lrc_handle = %llu\n",
                                (uint64_t)attention->clientHandle, (uint64_t)attention->flags,
                                (uint32_t)attention->bitmaskSize, uint64_t(attention->execQueueHandle), uint64_t(attention->lrcHandle));
        if (attention->base.seqno < newestAttSeqNo.load()) {
            PRINT_DEBUGGER_INFO_LOG("Dropping stale ATT event seqno=%llu\n", (uint64_t)attention->base.seqno);
        } else {
            handleAttentionEvent(attention);
        }
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVmBind)) {

        NEO::EuDebugEventVmBind *vmBind = reinterpret_cast<NEO::EuDebugEventVmBind *>(event);
        UNRECOVERABLE_IF(clientHandleToConnection.find(vmBind->clientHandle) == clientHandleToConnection.end());

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM_BIND client_handle = %llu vm_handle = %llu num_binds = %llu vmBindflag=%lu\n",
                                static_cast<uint64_t>(vmBind->clientHandle), static_cast<uint64_t>(vmBind->vmHandle),
                                static_cast<uint64_t>(vmBind->numBinds), static_cast<uint32_t>(vmBind->flags));

        auto &connection = clientHandleToConnection[vmBind->clientHandle];
        UNRECOVERABLE_IF(connection->vmBindMap.find(vmBind->base.seqno) != connection->vmBindMap.end());
        auto &vmBindData = connection->vmBindMap[vmBind->base.seqno];
        vmBindData.vmBind = *vmBind;
        vmBindData.pendingNumBinds = vmBind->numBinds;
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVmBindOp)) {
        NEO::EuDebugEventVmBindOp *vmBindOp = reinterpret_cast<NEO::EuDebugEventVmBindOp *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: drm_xe_eudebug_event_vm_bind_op vm_bind_ref_seqno = %llu num_extensions = %llu addr = 0x%llx range = %llu\n",
                                static_cast<uint64_t>(vmBindOp->vmBindRefSeqno), static_cast<uint64_t>(vmBindOp->numExtensions),
                                static_cast<uint64_t>(vmBindOp->addr), static_cast<uint64_t>(vmBindOp->range));

        auto &vmBindMap = clientHandleToConnection[clientHandle]->vmBindMap;
        UNRECOVERABLE_IF(vmBindMap.find(vmBindOp->vmBindRefSeqno) == vmBindMap.end());
        auto &vmBindData = vmBindMap[vmBindOp->vmBindRefSeqno];
        UNRECOVERABLE_IF(!vmBindData.pendingNumBinds);

        auto &vmBindOpData = vmBindData.vmBindOpMap[vmBindOp->base.seqno];
        vmBindOpData.pendingNumExtensions = vmBindOp->numExtensions;
        vmBindOpData.vmBindOp = *vmBindOp;
        vmBindData.pendingNumBinds--;
        clientHandleToConnection[clientHandle]->vmBindIdentifierMap[vmBindOp->base.seqno] = vmBindOp->vmBindRefSeqno;
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVmBindUfence)) {
        NEO::EuDebugEventVmBindUfence *vmBindUfence = reinterpret_cast<NEO::EuDebugEventVmBindUfence *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM_BIND_UFENCE vm_bind_ref_seqno = %llu\n",
                                static_cast<uint64_t>(vmBindUfence->vmBindRefSeqno));

        auto &vmBindMap = clientHandleToConnection[clientHandle]->vmBindMap;
        UNRECOVERABLE_IF(vmBindMap.find(vmBindUfence->vmBindRefSeqno) == vmBindMap.end());
        uint32_t uFenceRequired = vmBindMap[vmBindUfence->vmBindRefSeqno].vmBind.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventVmBindFlagUfence);
        UNRECOVERABLE_IF(!uFenceRequired);
        UNRECOVERABLE_IF(vmBindMap[vmBindUfence->vmBindRefSeqno].uFenceReceived); // Dont expect multiple UFENCE for same vm_bind
        vmBindMap[vmBindUfence->vmBindRefSeqno].uFenceReceived = true;
        vmBindMap[vmBindUfence->vmBindRefSeqno].vmBindUfence = *vmBindUfence;
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeMetadata)) {
        NEO::EuDebugEventMetadata *metaData = reinterpret_cast<NEO::EuDebugEventMetadata *>(event);
        if (clientHandle == invalidClientHandle) {
            clientHandle = metaData->clientHandle;
        }

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_METADATA client_handle = %llu metadata_handle = %llu type = %llu len = %llu\n",
                                (uint64_t)metaData->clientHandle, (uint64_t)metaData->metadataHandle, (uint64_t)metaData->type, (uint64_t)metaData->len);

        handleMetadataEvent(metaData);
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeVmBindOpMetadata)) {
        NEO::EuDebugEventVmBindOpMetadata *vmBindOpMetadata = reinterpret_cast<NEO::EuDebugEventVmBindOpMetadata *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM_BIND_OP_METADATA vm_bind_op_ref_seqno = %llu metadata_handle = %llu metadata_cookie = %llu\n",
                                static_cast<uint64_t>(vmBindOpMetadata->vmBindOpRefSeqno), static_cast<uint64_t>(vmBindOpMetadata->metadataHandle),
                                static_cast<uint64_t>(vmBindOpMetadata->metadataCookie));

        auto &vmBindMap = clientHandleToConnection[clientHandle]->vmBindMap;
        auto &vmBindIdentifierMap = clientHandleToConnection[clientHandle]->vmBindIdentifierMap;
        UNRECOVERABLE_IF(vmBindIdentifierMap.find(vmBindOpMetadata->vmBindOpRefSeqno) == vmBindIdentifierMap.end());
        VmBindSeqNo vmBindSeqNo = vmBindIdentifierMap[vmBindOpMetadata->vmBindOpRefSeqno];
        UNRECOVERABLE_IF(vmBindMap.find(vmBindSeqNo) == vmBindMap.end());
        auto &vmBindOpData = vmBindMap[vmBindSeqNo].vmBindOpMap[vmBindOpMetadata->vmBindOpRefSeqno];
        UNRECOVERABLE_IF(!vmBindOpData.pendingNumExtensions);
        vmBindOpData.vmBindOpMetadataVec.push_back(*vmBindOpMetadata);
        vmBindOpData.pendingNumExtensions--;
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypeExecQueuePlacements)) {
        NEO::EuDebugEventExecQueuePlacements *execQueuePlacements = reinterpret_cast<NEO::EuDebugEventExecQueuePlacements *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE_PLACEMENTS client_handle = %" SCNx64
                                " vm_handle = %" SCNx64
                                " exec_queue_handle = %" SCNx64
                                " lrc_handle = %" SCNx64
                                " num_placements = %" SCNx32
                                "\n ",
                                static_cast<uint64_t>(execQueuePlacements->clientHandle),
                                static_cast<uint64_t>(execQueuePlacements->vmHandle),
                                static_cast<uint64_t>(execQueuePlacements->execQueueHandle), static_cast<uint64_t>(execQueuePlacements->lrcHandle),
                                static_cast<uint32_t>(execQueuePlacements->numPlacements));

        UNRECOVERABLE_IF(execQueuePlacements->numPlacements == 0);
        auto engine = reinterpret_cast<NEO::XeEngineClassInstance *>(&(execQueuePlacements->instances[0]));
        auto tileIndex = DrmHelper::getTileIdFromGtId(connectedDevice, engine->gtId);
        UNRECOVERABLE_IF(tileIndex < 0);

        auto &vmToTile = clientHandleToConnection[execQueuePlacements->clientHandle]->vmToTile;
        if (vmToTile.find(execQueuePlacements->vmHandle) != vmToTile.end()) {
            if (vmToTile[execQueuePlacements->vmHandle] != static_cast<uint32_t>(tileIndex)) {
                PRINT_DEBUGGER_ERROR_LOG("vmToTile map: For vm_handle = %lu tileIndex = %u already present. Attempt to overwrite with tileIndex = %d\n",
                                         static_cast<uint64_t>(execQueuePlacements->vmHandle), vmToTile[execQueuePlacements->vmHandle], tileIndex);
                DEBUG_BREAK_IF(true);
            }
        } else {
            clientHandleToConnection[execQueuePlacements->clientHandle]->vmToTile[execQueuePlacements->vmHandle] = tileIndex;
            PRINT_DEBUGGER_INFO_LOG("clientHandleToConnection[%" SCNx64 "]->vmToTile[%" SCNx64 "] = %d\n",
                                    static_cast<uint64_t>(execQueuePlacements->clientHandle), static_cast<uint64_t>(execQueuePlacements->vmHandle), tileIndex);
        }
    } else if (type == euDebugInterface->getParamValue(NEO::EuDebugParam::eventTypePagefault)) {
        NEO::EuDebugEventPageFault *pf = reinterpret_cast<NEO::EuDebugEventPageFault *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_PAGEFAULT flags = %d, address = %llu seqno = %d, length = %llu"
                                " client_handle = %llu pf_flags = %llu  bitmask_size = %lu exec_queue_handle = %llu\n",
                                (int)pf->base.flags, (uint64_t)pf->pagefaultAddress, (uint64_t)pf->base.seqno, (uint64_t)pf->base.len,
                                (uint64_t)pf->clientHandle, (uint64_t)pf->flags, (uint32_t)pf->bitmaskSize, uint64_t(pf->execQueueHandle));
        auto tileIndex = 0u;
        auto vmHandle = getVmHandleFromClientAndlrcHandle(pf->clientHandle, pf->lrcHandle);
        if (vmHandle == invalidHandle) {
            return;
        }
        PageFaultEvent pfEvent = {vmHandle, tileIndex, pf->pagefaultAddress, pf->bitmaskSize, pf->bitmask};
        handlePageFaultEvent(pfEvent);
    } else {
        additionalEvents(event);
    }
}

void DebugSessionLinuxXe::processPendingVmBindEvents() {
    if (clientHandle == invalidClientHandle) {
        return;
    }
    std::vector<decltype(clientHandleToConnection[clientHandle]->vmBindMap)::key_type> keysToDelete;
    auto &vmBindMap = clientHandleToConnection[clientHandle]->vmBindMap;
    for (auto &pair : vmBindMap) {
        auto &vmBindData = pair.second;
        if (handleVmBind(vmBindData) == false) {
            break;
        }
        keysToDelete.push_back(pair.first);
    }
    for (const auto &key : keysToDelete) {
        vmBindMap.erase(key);
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
    uint32_t uFenceRequired = vmBindData.vmBind.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventVmBindFlagUfence);
    if (uFenceRequired) {
        if (!vmBindData.uFenceReceived) {
            return false;
        }
    }

    return true;
}

bool DebugSessionLinuxXe::handleVmBind(VmBindData &vmBindData) {
    if (!canHandleVmBind(vmBindData)) {
        return false;
    }
    bool shouldAckEvent = vmBindData.vmBind.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventVmBindFlagUfence);
    auto connection = clientHandleToConnection[clientHandle].get();
    auto elfHandleInVmBind = invalidHandle;

    uint64_t moduleHandle = invalidHandle;
    uint64_t isaAddr = 0;
    bool triggerModuleLoadEvent = false;

    uint32_t tileIndex = 0;
    auto numTiles = connectedDevice->getNEODevice()->getNumSubDevices();
    if (numTiles > 0) {
        if (connection->vmToTile.find(vmBindData.vmBind.vmHandle) != connection->vmToTile.end()) {
            tileIndex = connection->vmToTile[vmBindData.vmBind.vmHandle];
            PRINT_DEBUGGER_INFO_LOG("handleVmBind: tileIndex = %d for vmHandle = %" SCNx64 " \n", tileIndex, vmBindData.vmBind.vmHandle);
        } else {
            PRINT_DEBUGGER_ERROR_LOG("handleVmBind: tileIndex not found for vmHandle = %" SCNx64 " \n", vmBindData.vmBind.vmHandle);
            return false;
        }
    }

    for (auto &vmBindOpData : vmBindData.vmBindOpMap) {
        auto &vmBindOp = vmBindOpData.second.vmBindOp;
        for (const auto &vmBindOpMetadata : vmBindOpData.second.vmBindOpMetadataVec) {
            const NEO::DeviceBitfield devices(vmBindOpMetadata.metadataCookie);
            auto &metaDataEntry = connection->metaDataMap[vmBindOpMetadata.metadataHandle];
            if (vmBindOp.base.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitCreate)) {
                {
                    std::lock_guard<std::mutex> lock(asyncThreadMutex);
                    if (metaDataEntry.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataSbaArea)) {
                        connection->vmToStateBaseAreaBindInfo[vmBindData.vmBind.vmHandle] = {vmBindOp.addr, vmBindOp.range};
                    }
                    if (metaDataEntry.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataSipArea)) {
                        connection->vmToContextStateSaveAreaBindInfo[vmBindData.vmBind.vmHandle] = {vmBindOp.addr, vmBindOp.range};
                    }
                    if (metaDataEntry.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataModuleArea)) {
                        isaAddr = vmBindOp.addr;
                        if (connection->isaMap[tileIndex].find(vmBindOp.addr) == connection->isaMap[tileIndex].end()) {
                            auto &isaMap = connection->isaMap[tileIndex];
                            auto isa = std::make_unique<IsaAllocation>();
                            isa->bindInfo = {vmBindOp.addr, vmBindOp.range};
                            isa->vmHandle = vmBindData.vmBind.vmHandle;
                            isa->elfHandle = invalidHandle;
                            isa->moduleBegin = 0;
                            isa->moduleEnd = 0;
                            isa->tileInstanced = !(this->debugArea.isShared);
                            isa->validVMs.insert(vmBindData.vmBind.vmHandle);
                            uint32_t deviceBitfield = 0;
                            auto &debugModule = connection->metaDataToModule[vmBindOpMetadata.metadataHandle];
                            memcpy_s(&deviceBitfield, sizeof(uint32_t),
                                     &debugModule.deviceBitfield,
                                     sizeof(debugModule.deviceBitfield));
                            const NEO::DeviceBitfield devices(deviceBitfield);
                            isa->deviceBitfield = devices;
                            isa->perKernelModule = false;
                            isaMap[vmBindOp.addr] = std::move(isa);
                        }
                        connection->vmToModuleDebugAreaBindInfo[vmBindData.vmBind.vmHandle] = {vmBindOp.addr, vmBindOp.range};
                    }
                }

                if (metaDataEntry.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataElfBinary)) {
                    isaAddr = vmBindOp.addr;
                    if (connection->isaMap[tileIndex].find(vmBindOp.addr) == connection->isaMap[tileIndex].end()) {
                        auto &isaMap = connection->isaMap[tileIndex];
                        auto &elfMap = connection->elfMap;
                        auto isa = std::make_unique<IsaAllocation>();
                        isa->bindInfo = {vmBindOp.addr, vmBindOp.range};
                        isa->vmHandle = vmBindData.vmBind.vmHandle;
                        isa->elfHandle = vmBindOpMetadata.metadataHandle;
                        isa->moduleBegin = reinterpret_cast<uint64_t>(metaDataEntry.data.get());
                        isa->moduleEnd = isa->moduleBegin + metaDataEntry.metadata.len;
                        isa->tileInstanced = (devices.count() != 0);
                        isa->validVMs.insert(vmBindData.vmBind.vmHandle);
                        isa->perKernelModule = false;
                        isa->deviceBitfield = devices;
                        elfMap[isa->moduleBegin] = isa->elfHandle;
                        isaMap[vmBindOp.addr] = std::move(isa);
                        isaMap[vmBindOp.addr]->moduleLoadEventAck = true;
                        elfHandleInVmBind = vmBindOpMetadata.metadataHandle;
                    } else {
                        auto &isa = connection->isaMap[tileIndex][vmBindOp.addr];
                        isa->validVMs.insert(vmBindData.vmBind.vmHandle);
                    }
                }
                if (metaDataEntry.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataProgramModule)) {
                    auto &module = connection->metaDataToModule[vmBindOpMetadata.metadataHandle];
                    module.deviceBitfield = devices;
                    module.segmentVmBindCounter[tileIndex]++;
                    DEBUG_BREAK_IF(module.loadAddresses[tileIndex].size() > module.segmentCount);

                    bool canTriggerEvent = module.loadAddresses[tileIndex].size() == (module.segmentCount - 1);
                    module.loadAddresses[tileIndex].insert(vmBindOp.addr);
                    moduleHandle = vmBindOpMetadata.metadataHandle;
                    if (canTriggerEvent && module.loadAddresses[tileIndex].size() == module.segmentCount) {
                        bool allInstancesEventsReceived = true;
                        if (module.deviceBitfield.count() > 1) {
                            allInstancesEventsReceived = checkAllOtherTileModuleSegmentsPresent(tileIndex, module);
                        }
                        triggerModuleLoadEvent = allInstancesEventsReceived;
                    }
                }
            }
        }

        if (vmBindOp.base.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitDestroy)) {
            if (connection->isaMap[tileIndex].count(vmBindOp.addr)) {
                auto &isa = connection->isaMap[tileIndex][vmBindOp.addr];
                if (isa->validVMs.count(vmBindData.vmBind.vmHandle)) {
                    auto &module = connection->metaDataToModule[isa->moduleHandle];
                    module.segmentVmBindCounter[tileIndex]--;
                    if (module.segmentVmBindCounter[tileIndex] == 0) {
                        zet_debug_event_t debugEvent = {};
                        auto &metaDataEntry = connection->metaDataMap[isa->moduleHandle];
                        auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                        auto loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
                        debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD;
                        debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                        debugEvent.info.module.load = loadAddress;
                        auto &elfMetadata = connection->metaDataMap[isa->elfHandle];
                        debugEvent.info.module.moduleBegin = reinterpret_cast<uint64_t>(elfMetadata.data.get());
                        debugEvent.info.module.moduleEnd = reinterpret_cast<uint64_t>(elfMetadata.data.get()) + elfMetadata.metadata.len;

                        bool notifyEvent = true;
                        if (module.deviceBitfield.count() > 1) {
                            notifyEvent = checkAllOtherTileModuleSegmentsRemoved(tileIndex, module);
                        }
                        if (notifyEvent) {
                            pushApiEvent(debugEvent, metaDataEntry.metadata.metadataHandle);
                        }
                        module.loadAddresses[tileIndex].clear();
                        module.moduleLoadEventAcked[tileIndex] = false;
                    }
                    isa->validVMs.erase(vmBindData.vmBind.vmHandle);
                }
            }
        }
    }

    if (isaAddr && moduleHandle != invalidHandle) {
        connection->isaMap[tileIndex][isaAddr]->moduleHandle = moduleHandle;
    }

    if (triggerModuleLoadEvent) {
        DEBUG_BREAK_IF(moduleHandle == invalidHandle);
        DEBUG_BREAK_IF(elfHandleInVmBind == invalidHandle);
        auto &metaDataEntry = connection->metaDataMap[moduleHandle];
        auto &module = connection->metaDataToModule[moduleHandle];
        auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
        auto loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
        PRINT_DEBUGGER_INFO_LOG("Zebin module loaded at: %p, with %u isa allocations", (void *)loadAddress, module.segmentCount);

        auto &elfMetadata = connection->metaDataMap[elfHandleInVmBind];
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
        debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
        debugEvent.info.module.load = loadAddress;
        debugEvent.info.module.moduleBegin = reinterpret_cast<uint64_t>(elfMetadata.data.get());
        debugEvent.info.module.moduleEnd = reinterpret_cast<uint64_t>(elfMetadata.data.get()) + elfMetadata.metadata.len;
        debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;

        {
            std::lock_guard<std::mutex> lock(asyncThreadMutex);
            if (vmBindData.vmBind.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventVmBindFlagUfence)) {
                if (vmBindData.vmBindUfence.base.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitNeedAck)) {
                    EventToAck ackEvent(vmBindData.vmBindUfence.base.seqno, vmBindData.vmBindUfence.base.type);
                    module.ackEvents[tileIndex].push_back(ackEvent);
                    shouldAckEvent = false;
                }
            }
        }
        pushApiEvent(debugEvent, metaDataEntry.metadata.metadataHandle);
    }

    if (shouldAckEvent && (vmBindData.vmBindUfence.base.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitNeedAck))) {
        EventToAck ackEvent(vmBindData.vmBindUfence.base.seqno, vmBindData.vmBindUfence.base.type);
        eventAckIoctl(ackEvent);
    }

    return true;
}

void DebugSessionLinuxXe::handleMetadataEvent(NEO::EuDebugEventMetadata *metaData) {
    bool destroy = metaData->base.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitDestroy);
    bool create = metaData->base.flags & euDebugInterface->getParamValue(NEO::EuDebugParam::eventBitCreate);

    UNRECOVERABLE_IF(clientHandleToConnection.find(metaData->clientHandle) == clientHandleToConnection.end());
    const auto &connection = clientHandleToConnection[metaData->clientHandle];

    if (destroy && connection->metaDataMap[metaData->metadataHandle].metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataProgramModule)) {
        DEBUG_BREAK_IF(connection->metaDataToModule[metaData->metadataHandle].segmentVmBindCounter[0] != 0 ||
                       connection->metaDataToModule[metaData->metadataHandle].loadAddresses[0].size() > 0);

        connection->metaDataToModule.erase(metaData->metadataHandle);
    }

    if (create) {
        if (!metaData->len) {
            connection->metaDataMap[metaData->metadataHandle].metadata = *metaData;
            return;
        }

        NEO::EuDebugReadMetadata readMetadata{};
        auto ptr = std::make_unique<char[]>(metaData->len);
        readMetadata.clientHandle = metaData->clientHandle;
        readMetadata.metadataHandle = static_cast<decltype(readMetadata.metadataHandle)>(metaData->metadataHandle);
        readMetadata.ptr = reinterpret_cast<uint64_t>(ptr.get());
        readMetadata.size = metaData->len;
        auto ret = ioctl(euDebugInterface->getParamValue(NEO::EuDebugParam::ioctlReadMetadata), &readMetadata);
        if (ret != 0) {
            PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_METADATA ret = %d errno = %d\n", ret, errno);
            return;
        }

        connection->metaDataMap[metaData->metadataHandle].metadata = *metaData;
        connection->metaDataMap[metaData->metadataHandle].data = std::move(ptr);

        if (metaData->type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataProgramModule)) {
            auto handle = metaData->metadataHandle;
            auto &newModule = connection->metaDataToModule[handle];
            newModule.segmentCount = 0;
            newModule.moduleHandle = handle;
            for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                newModule.segmentVmBindCounter[i] = 0;
                newModule.loadAddresses[i].clear();
                newModule.moduleLoadEventAcked[i] = false;
            }
        }
        extractMetaData(metaData->clientHandle, connection->metaDataMap[metaData->metadataHandle]);
    }
}

void DebugSessionLinuxXe::extractMetaData(uint64_t client, const MetaData &metaData) {
    if (metaData.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataSbaArea) ||
        metaData.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataModuleArea) ||
        metaData.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataSipArea)) {
        UNRECOVERABLE_IF(metaData.metadata.len != 8);
        uint64_t *data = (uint64_t *)metaData.data.get();

        if (metaData.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataSbaArea)) {
            clientHandleToConnection[client]->stateBaseAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("SbaTrackingBuffer GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->stateBaseAreaGpuVa);
        }
        if (metaData.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataModuleArea)) {
            clientHandleToConnection[client]->moduleDebugAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ModuleHeapDebugArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->moduleDebugAreaGpuVa);
        }
        if (metaData.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataSipArea)) {
            clientHandleToConnection[client]->contextStateSaveAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ContextSaveArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->contextStateSaveAreaGpuVa);
        }
    }

    if (metaData.metadata.type == euDebugInterface->getParamValue(NEO::EuDebugParam::metadataProgramModule)) {
        uint32_t segmentCount = 0;
        memcpy_s(&segmentCount, sizeof(uint32_t), metaData.data.get(), metaData.metadata.len);
        clientHandleToConnection[client]->metaDataToModule[metaData.metadata.metadataHandle].segmentCount = segmentCount;
        PRINT_DEBUGGER_INFO_LOG("Zebin module = %ull, segment count = %ul", metaData.metadata.metadataHandle, segmentCount);
    }
}

int DebugSessionLinuxXe::openVmFd(uint64_t vmHandle, [[maybe_unused]] bool readOnly) {
    NEO::EuDebugVmOpen vmOpen = {
        .extensions = 0,
        .clientHandle = clientHandle,
        .vmHandle = vmHandle,
        .flags = 0,
        .timeoutNs = 5000000000u};

    return ioctl(euDebugInterface->getParamValue(NEO::EuDebugParam::ioctlVmOpen), &vmOpen);
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

void DebugSessionLinuxXe::handleAttentionEvent(NEO::EuDebugEventEuAttention *attention) {
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
    attentionEventFields.bitmaskSize = attention->bitmaskSize;
    attentionEventFields.clientHandle = attention->clientHandle;
    attentionEventFields.contextHandle = attention->execQueueHandle;
    attentionEventFields.lrcHandle = attention->lrcHandle;

    if (updateStoppedThreadsAndCheckTriggerEvents(attentionEventFields, 0, threadsWithAttention) != ZE_RESULT_SUCCESS) {
        PRINT_DEBUGGER_ERROR_LOG("Failed to update stopped threads and check trigger events\n", "");
    }
}

int DebugSessionLinuxXe::threadControlInterruptAll() {
    int euControlRetVal = -1;
    NEO::EuDebugEuControl euControl = {};
    euControl.clientHandle = clientHandle;
    euControl.cmd = euDebugInterface->getParamValue(NEO::EuDebugParam::euControlCmdInterruptAll);
    euControl.bitmaskSize = 0;
    euControl.bitmaskPtr = 0;

    DEBUG_BREAK_IF(clientHandleToConnection.find(clientHandle) == clientHandleToConnection.end());
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    for (const auto &execQueue : clientHandleToConnection[clientHandle]->execQueues) {
        euControl.execQueueHandle = execQueue.first;
        for (const auto &lrcHandle : execQueue.second.lrcHandles) {
            euControl.lrcHandle = lrcHandle;
            euControlRetVal = ioctl(euDebugInterface->getParamValue(NEO::EuDebugParam::ioctlEuControl), &euControl);
            if (euControlRetVal != 0) {
                PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d, execQueueHandle = %llu lrcHandle = %llu\n",
                                         euControlRetVal, errno, static_cast<uint32_t>(euControl.cmd), static_cast<uint64_t>(euControl.execQueueHandle),
                                         static_cast<uint64_t>(euControl.lrcHandle));
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

int DebugSessionLinuxXe::threadControlStopped(std::unique_ptr<uint8_t[]> &bitmaskOut, size_t &bitmaskSizeOut) {
    int euControlRetVal = -1;
    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();
    NEO::EuDebugEuControl euControl = {};
    euControl.clientHandle = clientHandle;
    euControl.cmd = euDebugInterface->getParamValue(NEO::EuDebugParam::euControlCmdStopped);

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({}, hwInfo, bitmask, bitmaskSize);
    euControl.bitmaskSize = static_cast<uint32_t>(bitmaskSize);
    euControl.bitmaskPtr = reinterpret_cast<uint64_t>(bitmask.get());

    DEBUG_BREAK_IF(clientHandleToConnection.find(clientHandle) == clientHandleToConnection.end());
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    for (const auto &execQueue : clientHandleToConnection[clientHandle]->execQueues) {
        euControl.execQueueHandle = execQueue.first;
        for (const auto &lrcHandle : execQueue.second.lrcHandles) {
            euControl.lrcHandle = lrcHandle;
            euControlRetVal = ioctl(euDebugInterface->getParamValue(NEO::EuDebugParam::ioctlEuControl), &euControl);
            if (euControlRetVal != 0) {
                PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d, execQueueHandle = %llu lrcHandle = %llu\n",
                                         euControlRetVal, errno, static_cast<uint32_t>(euControl.cmd), static_cast<uint64_t>(euControl.execQueueHandle),
                                         static_cast<uint64_t>(euControl.lrcHandle));
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
    bitmaskSizeOut = euControl.bitmaskSize;
    return euControlRetVal;
}

int DebugSessionLinuxXe::threadControlResume(const std::vector<EuThread::ThreadId> &threads) {
    int euControlRetVal = -1;
    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();
    NEO::EuDebugEuControl euControl = {};
    euControl.clientHandle = clientHandle;
    euControl.bitmaskSize = 0;
    euControl.bitmaskPtr = 0;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);
    euControl.bitmaskSize = static_cast<uint32_t>(bitmaskSize);
    euControl.bitmaskPtr = reinterpret_cast<uint64_t>(bitmask.get());
    printBitmask(bitmask.get(), bitmaskSize);
    uint64_t execQueueHandle{0};
    uint64_t lrcHandle{0};
    allThreads[threads[0]]->getContextHandle(execQueueHandle);
    allThreads[threads[0]]->getLrcHandle(lrcHandle);
    euControl.execQueueHandle = execQueueHandle;
    euControl.lrcHandle = lrcHandle;

    auto invokeIoctl = [&](int cmd) {
        euControl.cmd = cmd;
        euControlRetVal = ioctl(euDebugInterface->getParamValue(NEO::EuDebugParam::ioctlEuControl), &euControl);
        if (euControlRetVal != 0) {
            PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d, execQueueHandle = %llu lrcHandle = %llu\n",
                                     euControlRetVal, errno, static_cast<uint32_t>(euControl.cmd), static_cast<uint64_t>(euControl.execQueueHandle),
                                     static_cast<uint64_t>(euControl.lrcHandle));
        } else {
            PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_EU_CONTROL: seqno = %llu command = %u\n", static_cast<uint64_t>(euControl.seqno), static_cast<uint32_t>(euControl.cmd));
        }
    };

    if (l0GfxCoreHelper.threadResumeRequiresUnlock()) {
        invokeIoctl(getEuControlCmdUnlock());
    }

    invokeIoctl(euDebugInterface->getParamValue(NEO::EuDebugParam::euControlCmdResume));
    return euControlRetVal;
}

int DebugSessionLinuxXe::threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile,
                                       ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmaskOut, size_t &bitmaskSizeOut) {

    bitmaskSizeOut = 0;
    switch (threadCmd) {
    case ThreadControlCmd::interruptAll:
        return threadControlInterruptAll();
    case ThreadControlCmd::resume:
        return threadControlResume(threads);
    case ThreadControlCmd::stopped:
        return threadControlStopped(bitmaskOut, bitmaskSizeOut);
    default:
        break;
    }
    return -1;
}

void DebugSessionLinuxXe::updateContextAndLrcHandlesForThreadsWithAttention(EuThread::ThreadId threadId, const AttentionEventFields &attention) {
    allThreads[threadId]->setContextHandle(attention.contextHandle);
    allThreads[threadId]->setLrcHandle(attention.lrcHandle);
}

int DebugSessionLinuxXe::eventAckIoctl(EventToAck &event) {
    NEO::EuDebugAckEvent eventToAck = {};
    eventToAck.type = event.type;
    eventToAck.seqno = event.seqno;
    eventToAck.flags = 0;
    auto ret = ioctl(euDebugInterface->getParamValue(NEO::EuDebugParam::ioctlAckEvent), &eventToAck);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_ACK_EVENT seqno = %llu ret = %d errno = %d\n", static_cast<uint64_t>(eventToAck.seqno), ret, ret != 0 ? errno : 0);
    return ret;
}

} // namespace L0
