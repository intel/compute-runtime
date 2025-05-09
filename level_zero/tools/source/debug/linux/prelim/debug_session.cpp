/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_thread.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/debug/linux/debug_session_factory.h"
#include "level_zero/tools/source/debug/linux/drm_helper.h"
#include "level_zero/zet_intel_gpu_debug.h"
#include <level_zero/ze_api.h>

#include "common/StateSaveAreaHeader.h"

#include <algorithm>
#include <fcntl.h>

namespace L0 {

static DebugSessionLinuxPopulateFactory<DEBUG_SESSION_LINUX_TYPE_I915, DebugSessionLinuxi915>
    populatei915Debugger;

DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd, void *params);

DebugSessionLinuxi915::DebugSessionLinuxi915(const zet_debug_config_t &config, Device *device, int debugFd, void *params) : DebugSessionLinux(config, device, debugFd) {
    ioctlHandler.reset(new IoctlHandleri915);

    if (params) {
        this->i915DebuggerVersion = reinterpret_cast<prelim_drm_i915_debugger_open_param *>(params)->version;
    }

    if (this->i915DebuggerVersion >= 3) {
        this->blockOnFenceMode = true;
    }

    for (size_t i = 0; i < arrayCount(euControlInterruptSeqno); i++) {
        euControlInterruptSeqno[i] = invalidHandle;
    }
};
DebugSessionLinuxi915::~DebugSessionLinuxi915() {
    closeAsyncThread();
    closeInternalEventsThread();
    for (auto &session : tileSessions) {
        delete session.first;
    }
    tileSessions.resize(0);
    closeFd();
}

DebugSession *DebugSessionLinuxi915::createLinuxSession(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {
    struct prelim_drm_i915_debugger_open_param open = {};
    open.pid = config.pid;
    open.events = 0;
    open.version = 0;
    auto debugFd = DrmHelper::ioctl(device, NEO::DrmIoctl::debuggerOpen, &open);
    if (debugFd >= 0) {
        PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN: open.pid: %d, open.events: %d, debugFd: %d\n",
                                open.pid, open.events, debugFd);

        return createDebugSessionHelper(config, device, debugFd, &open);
    } else {
        auto reason = DrmHelper::getErrno(device);
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN failed: open.pid: %d, open.events: %d, retCode: %d, errno: %d\n",
                                 open.pid, open.events, debugFd, reason);
        result = translateDebuggerOpenErrno(reason);
    }
    return nullptr;
}

int DebugSessionLinuxi915::ioctl(unsigned long request, void *arg) {
    return ioctlHandler->ioctl(fd, request, arg);
}

int DebugSessionLinuxi915::openVmFd(uint64_t vmHandle, bool readOnly) {
    uint64_t flags = static_cast<decltype(prelim_drm_i915_debug_vm_open::flags)>(readOnly ? PRELIM_I915_DEBUG_VM_OPEN_READ_ONLY : PRELIM_I915_DEBUG_VM_OPEN_READ_WRITE);
    prelim_drm_i915_debug_vm_open vmOpen = {
        .client_handle = static_cast<decltype(prelim_drm_i915_debug_vm_open::client_handle)>(clientHandle),
        .handle = static_cast<decltype(prelim_drm_i915_debug_vm_open::handle)>(vmHandle),
        .flags = flags};

    return ioctl(PRELIM_I915_DEBUG_IOCTL_VM_OPEN, &vmOpen);
}

DebugSessionImp *DebugSessionLinuxi915::createTileSession(const zet_debug_config_t &config, Device *device, DebugSessionImp *rootDebugSession) {
    auto tileSession = new TileDebugSessionLinuxi915(config, device, rootDebugSession);
    tileSession->initialize();
    return tileSession;
}

void *DebugSessionLinuxi915::asyncThreadFunction(void *arg) {
    DebugSessionLinuxi915 *self = reinterpret_cast<DebugSessionLinuxi915 *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger async thread start\n", "");

    while (self->asyncThread.threadActive) {
        self->handleEventsAsync();

        if (self->tileSessionsEnabled) {
            for (size_t tileIndex = 0; tileIndex < self->tileSessions.size(); tileIndex++) {
                static_cast<TileDebugSessionLinuxi915 *>(self->tileSessions[tileIndex].first)->generateEventsAndResumeStoppedThreads();
                static_cast<TileDebugSessionLinuxi915 *>(self->tileSessions[tileIndex].first)->sendInterrupts();
            }
        } else {
            self->generateEventsAndResumeStoppedThreads();
            self->sendInterrupts();
        }
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread closing\n", "");

    return nullptr;
}

void DebugSessionLinuxi915::startAsyncThread() {
    asyncThread.thread = NEO::Thread::createFunc(asyncThreadFunction, reinterpret_cast<void *>(this));
}

bool DebugSessionLinuxi915::handleInternalEvent() {
    auto eventMemory = getInternalEvent();
    if (eventMemory == nullptr) {
        return false;
    }

    auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(eventMemory.get());
    handleEvent(debugEvent);

    if (debugEvent->type != PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND && pendingVmBindEvents.size() > 0) {
        processPendingVmBindEvents();
    }
    return true;
}

void DebugSessionLinuxi915::readInternalEventsAsync() {

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

        if (tileSessionsEnabled) {
            auto numTiles = connectedDevice->getNEODevice()->getNumSubDevices();
            for (uint32_t tileIndex = 0; tileIndex < numTiles; tileIndex++) {
                static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent);
                static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->detached = true;
            }
        } else {
            pushApiEvent(debugEvent);
        }
        detached = true;
    } else if (numberOfFds > 0) {

        ze_result_t result = ZE_RESULT_SUCCESS;

        int maxLoopCount = 3;
        do {

            uint8_t maxEventBuffer[sizeof(prelim_drm_i915_debug_event) + maxEventSize];
            auto event = reinterpret_cast<prelim_drm_i915_debug_event *>(maxEventBuffer);
            event->size = maxEventSize;
            event->type = PRELIM_DRM_I915_DEBUG_EVENT_READ;
            event->flags = 0;

            result = readEventImp(event);
            maxLoopCount--;

            if (result == ZE_RESULT_SUCCESS) {
                std::lock_guard<std::mutex> lock(internalEventThreadMutex);

                auto memory = std::make_unique<uint64_t[]>(maxEventSize / sizeof(uint64_t));
                memcpy(memory.get(), event, maxEventSize);

                internalEventQueue.push(std::move(memory));
                internalEventCondition.notify_one();
            }
        } while (result == ZE_RESULT_SUCCESS && maxLoopCount > 0);
    }
}

void DebugSessionLinuxi915::handleEvent(prelim_drm_i915_debug_event *event) {
    auto type = event->type;

    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type = %lu flags = %d seqno = %llu size = %llu",
                            (uint32_t)event->type, (int)event->flags, (uint64_t)event->seqno, (uint64_t)event->size);

    switch (type) {
    case PRELIM_DRM_I915_DEBUG_EVENT_CLIENT: {
        auto clientEvent = reinterpret_cast<prelim_drm_i915_debug_event_client *>(event);

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
            DEBUG_BREAK_IF(clientHandleToConnection.find(clientEvent->handle) != clientHandleToConnection.end());
            clientHandleToConnection[clientEvent->handle].reset(new ClientConnectioni915);
            clientHandleToConnection[clientEvent->handle]->client = *clientEvent;
        }

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
            clientHandleClosed = clientEvent->handle;
        }

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_CLIENT flags = %d size = %llu client.handle = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)clientEvent->handle);

    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT: {
        prelim_drm_i915_debug_event_context *context = reinterpret_cast<prelim_drm_i915_debug_event_context *>(event);

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(context->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[context->client_handle]->contextsCreated[context->handle].handle = context->handle;
        }

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
            clientHandleToConnection[context->client_handle]->contextsCreated.erase(context->handle);
        }

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT flags = %d size = %llu client_handle = %llu handle = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)context->client_handle, (uint64_t)context->handle);
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_UUID: {
        prelim_drm_i915_debug_event_uuid *uuid = reinterpret_cast<prelim_drm_i915_debug_event_uuid *>(event);

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_UUID flags = %d size = %llu client_handle = %llu handle = %llu class_handle = %llu payload_size = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)uuid->client_handle, (uint64_t)uuid->handle, (uint64_t)uuid->class_handle, (uint64_t)uuid->payload_size);

        bool destroy = event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        bool create = event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE;

        if (destroy && clientHandleToConnection[uuid->client_handle]->uuidMap[uuid->handle].classIndex == NEO::DrmResourceClass::l0ZebinModule) {
            DEBUG_BREAK_IF(clientHandleToConnection[uuid->client_handle]->uuidToModule[uuid->handle].segmentVmBindCounter[0] != 0 ||
                           clientHandleToConnection[uuid->client_handle]->uuidToModule[uuid->handle].loadAddresses[0].size() > 0);

            clientHandleToConnection[uuid->client_handle]->uuidToModule.erase(uuid->handle);
        }

        if (destroy && (clientHandle == uuid->client_handle)) {

            for (const auto &uuidToDevice : uuidL0CommandQueueHandleToDevice) {
                if (uuidToDevice.first == uuid->handle) {
                    auto deviceIndex = uuidToDevice.second;
                    uuidL0CommandQueueHandleToDevice.erase(uuidToDevice.first);

                    zet_debug_event_t debugEvent = {};
                    debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT;

                    if (tileSessionsEnabled) {
                        auto tileSession = reinterpret_cast<TileDebugSessionLinuxi915 *>(tileSessions[deviceIndex].first);
                        tileSession->processExit();
                        tileSession->pushApiEvent(debugEvent);
                    } else if (uuidL0CommandQueueHandleToDevice.size() == 0) {
                        pushApiEvent(debugEvent);
                    }

                    break;
                }
            }
            break;
        }

        if (create) {
            const auto &connection = clientHandleToConnection[uuid->client_handle];
            if (uuid->payload_size) {
                prelim_drm_i915_debug_read_uuid readUuid = {};
                auto payload = std::make_unique<char[]>(uuid->payload_size);
                readUuid.client_handle = uuid->client_handle;
                readUuid.handle = static_cast<decltype(readUuid.handle)>(uuid->handle);
                readUuid.payload_ptr = reinterpret_cast<uint64_t>(payload.get());
                readUuid.payload_size = uuid->payload_size;
                auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_READ_UUID, &readUuid);

                if (ret == 0) {
                    std::string uuidString = std::string(readUuid.uuid, 36);
                    uint32_t classIndex = static_cast<uint32_t>(NEO::DrmResourceClass::maxSize);
                    auto validClassUuid = NEO::DrmUuid::getClassUuidIndex(uuidString, classIndex);

                    if (uuidString == NEO::uuidL0CommandQueueHash) {
                        if ((clientHandle == invalidClientHandle) || (clientHandle == uuid->client_handle)) {
                            clientHandle = uuid->client_handle;

                            uint32_t deviceIndex = 0;
                            if (readUuid.payload_size == sizeof(NEO::DebuggerL0::CommandQueueNotification)) {
                                auto notification = reinterpret_cast<NEO::DebuggerL0::CommandQueueNotification *>(payload.get());
                                deviceIndex = notification->subDeviceIndex;
                                UNRECOVERABLE_IF(notification->subDeviceCount > 0 && notification->subDeviceIndex >= notification->subDeviceCount);
                            }

                            zet_debug_event_t debugEvent = {};
                            debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;

                            if (tileSessionsEnabled) {
                                UNRECOVERABLE_IF(uuidL0CommandQueueHandleToDevice.find(uuid->handle) != uuidL0CommandQueueHandleToDevice.end());
                                auto tileSession = static_cast<TileDebugSessionLinuxi915 *>(tileSessions[deviceIndex].first);
                                tileSession->processEntry();
                                tileSession->pushApiEvent(debugEvent);
                            } else if (uuidL0CommandQueueHandleToDevice.size() == 0) {
                                pushApiEvent(debugEvent);
                            }
                            uuidL0CommandQueueHandleToDevice[uuid->handle] = deviceIndex;
                        }
                    }

                    if (validClassUuid) {
                        if (clientHandle == invalidClientHandle) {
                            clientHandle = uuid->client_handle;
                        }
                        std::string className(reinterpret_cast<char *>(readUuid.payload_ptr), readUuid.payload_size);
                        connection->classHandleToIndex[uuid->handle] = {className, classIndex};
                    } else {
                        auto &uuidData = connection->uuidMap[uuid->handle];
                        uuidData.classHandle = uuid->class_handle;
                        uuidData.handle = uuid->handle;
                        uuidData.data = std::move(payload);
                        uuidData.dataSize = uuid->payload_size;
                        uuidData.classIndex = NEO::DrmResourceClass::maxSize;

                        const auto indexIt = connection->classHandleToIndex.find(uuid->class_handle);
                        if (indexIt != connection->classHandleToIndex.end()) {
                            uuidData.classIndex = static_cast<NEO::DrmResourceClass>(indexIt->second.second);
                        }

                        if (uuidData.classIndex == NEO::DrmResourceClass::elf) {
                            auto cpuVa = extractVaFromUuidString(uuidString);
                            uuidData.ptr = cpuVa;
                        }

                        if (uuidData.classIndex == NEO::DrmResourceClass::l0ZebinModule) {
                            uint64_t handle = uuid->handle;

                            auto &newModule = connection->uuidToModule[handle];
                            newModule.segmentCount = 0;
                            newModule.moduleHandle = handle;
                            for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                                newModule.segmentVmBindCounter[i] = 0;
                                newModule.loadAddresses[i].clear();
                                newModule.moduleLoadEventAcked[i] = false;
                            }
                        }
                        extractUuidData(uuid->client_handle, uuidData);
                    }

                    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_UUID client_handle = %llu handle = %llu flags = %d uuid = %s ret = %d\n",
                                            (uint64_t)readUuid.client_handle, (uint64_t)readUuid.handle, (int)readUuid.flags, uuidString.c_str(), ret);
                } else {
                    PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_READ_UUID ret = %d errno = %d\n", ret, errno);
                }
            } else {
                connection->uuidMap[uuid->handle].classHandle = uuid->class_handle;
                connection->uuidMap[uuid->handle].handle = uuid->handle;
            }
        }

    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_VM: {
        prelim_drm_i915_debug_event_vm *vm = reinterpret_cast<prelim_drm_i915_debug_event_vm *>(event);

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_VM flags = %d size = %llu client_handle = %llu handle = %llu\n",
                                (int)event->flags, (uint64_t)event->size, (uint64_t)vm->client_handle, (uint64_t)vm->handle);

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->client_handle]->vmIds.emplace(static_cast<uint64_t>(vm->handle));
        }

        if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
            UNRECOVERABLE_IF(clientHandleToConnection.find(vm->client_handle) == clientHandleToConnection.end());
            clientHandleToConnection[vm->client_handle]->vmIds.erase(static_cast<uint64_t>(vm->handle));
        }
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND: {
        prelim_drm_i915_debug_event_vm_bind *vmBind = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(event);

        if (!handleVmBindEvent(vmBind)) {
            if (event->flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                prelim_drm_i915_debug_event_ack eventToAck = {};
                eventToAck.type = vmBind->base.type;
                eventToAck.seqno = vmBind->base.seqno;
                eventToAck.flags = 0;
                auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
                PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu ret = %d errno = %d\n", (uint64_t)eventToAck.seqno, ret, ret != 0 ? errno : 0);
            } else {
                auto sizeAligned = alignUp(event->size, sizeof(uint64_t));
                auto pendingEvent = std::make_unique<uint64_t[]>(sizeAligned / sizeof(uint64_t));
                memcpy_s(pendingEvent.get(), sizeAligned, event, event->size);
                pendingVmBindEvents.push_back(std::move(pendingEvent));
            }
        }

    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM: {
        prelim_drm_i915_debug_event_context_param *contextParam = reinterpret_cast<prelim_drm_i915_debug_event_context_param *>(event);
        handleContextParamEvent(contextParam);
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION: {
        prelim_drm_i915_debug_event_eu_attention *attention = reinterpret_cast<prelim_drm_i915_debug_event_eu_attention *>(event);

        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION flags = %d, seqno = %d, size = %llu"
                                " client_handle = %llu flags = %llu class = %lu instance = %lu bitmask_size = %lu ctx_handle = %llu\n",
                                (int)attention->base.flags, (uint64_t)attention->base.seqno, (uint64_t)attention->base.size,
                                (uint64_t)attention->client_handle, (uint64_t)attention->flags, (uint32_t)attention->ci.engine_class,
                                (uint32_t)attention->ci.engine_instance, (uint32_t)attention->bitmask_size, uint64_t(attention->ctx_handle));
        handleAttentionEvent(attention);
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_ENGINES: {
        prelim_drm_i915_debug_event_engines *engines = reinterpret_cast<prelim_drm_i915_debug_event_engines *>(event);
        handleEnginesEvent(engines);
    } break;

    case PRELIM_DRM_I915_DEBUG_EVENT_PAGE_FAULT: {
        prelim_drm_i915_debug_event_page_fault *pf = reinterpret_cast<prelim_drm_i915_debug_event_page_fault *>(event);
        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_PAGE_FAULT flags = %d, address = %llu seqno = %d, size = %llu"
                                " client_handle = %llu flags = %llu class = %lu instance = %lu bitmask_size = %lu ctx_handle = %llu\n",
                                (int)pf->base.flags, (uint64_t)pf->page_fault_address, (uint64_t)pf->base.seqno, (uint64_t)pf->base.size,
                                (uint64_t)pf->client_handle, (uint64_t)pf->flags, (uint32_t)pf->ci.engine_class,
                                (uint32_t)pf->ci.engine_instance, (uint32_t)pf->bitmask_size, uint64_t(pf->ctx_handle));
        NEO::EngineClassInstance engineClassInstance = {pf->ci.engine_class, pf->ci.engine_instance};
        auto tileIndex = DrmHelper::getEngineTileIndex(connectedDevice, engineClassInstance);
        auto vmHandle = getVmHandleFromClientAndlrcHandle(pf->client_handle, pf->lrc_handle);
        if (vmHandle == invalidHandle) {
            return;
        }
        PageFaultEvent pfEvent = {vmHandle, tileIndex, pf->page_fault_address, pf->bitmask_size, pf->bitmask};
        handlePageFaultEvent(pfEvent);
    } break;

    default:
        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: UNHANDLED %d flags = %d size = %llu\n", (int)event->type, (int)event->flags, (uint64_t)event->size);
        break;
    }
}

void DebugSessionLinuxi915::processPendingVmBindEvents() {
    size_t processedEvents = 0;
    for (size_t index = 0; index < pendingVmBindEvents.size(); index++) {
        auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(pendingVmBindEvents[index].get());
        if (handleVmBindEvent(debugEvent) == false) {
            break;
        }
        processedEvents++;
    }
    if (processedEvents > 0) {
        pendingVmBindEvents.erase(pendingVmBindEvents.begin(), pendingVmBindEvents.begin() + processedEvents);
    }
}

ze_result_t DebugSessionLinuxi915::readEventImp(prelim_drm_i915_debug_event *drmDebugEvent) {
    auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_READ_EVENT, drmDebugEvent);
    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT failed: retCode: %d errno = %d\n", ret, errno);
        return ZE_RESULT_NOT_READY;
    } else if (drmDebugEvent->flags & ~static_cast<uint32_t>(PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_DESTROY | PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK)) {
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT unsupported flag = %d\n", (int)drmDebugEvent->flags);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

bool DebugSessionLinuxi915::handleVmBindEvent(prelim_drm_i915_debug_event_vm_bind *vmBind) {

    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND flags = %d size = %llu client_handle = %llu vm_handle = %llu va_start = %p va_lenght = %llu num_uuids = %lu\n",
                            (int)vmBind->base.flags, (uint64_t)vmBind->base.size, (uint64_t)vmBind->client_handle, (uint64_t)vmBind->vm_handle, (void *)vmBind->va_start, (uint64_t)vmBind->va_length, (uint32_t)vmBind->num_uuids);

    const bool createEvent = (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE);
    const bool destroyEvent = (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY);

    bool shouldAckEvent = true;

    if (vmBind->num_uuids > 0 && vmBind->base.size > sizeof(prelim_drm_i915_debug_event_vm_bind)) {
        auto vmHandle = vmBind->vm_handle;
        auto connection = clientHandleToConnection[vmBind->client_handle].get();
        uint32_t index = 0;
        const auto uuid = vmBind->uuids[index];

        if (connection->uuidMap.find(uuid) == connection->uuidMap.end()) {
            PRINT_DEBUGGER_ERROR_LOG("Unknown UUID handle = %llu\n", (uint64_t)uuid);
            return false;
        }

        if (connection->vmToTile.find(vmHandle) == connection->vmToTile.end()) {
            DEBUG_BREAK_IF(connection->vmToTile.find(vmHandle) == connection->vmToTile.end() && (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) &&
                           (connection->uuidMap[uuid].classIndex == NEO::DrmResourceClass::isa || connection->uuidMap[uuid].classIndex == NEO::DrmResourceClass::moduleHeapDebugArea));
            return false;
        }

        const auto tileIndex = connection->vmToTile[vmHandle];

        PRINT_DEBUGGER_INFO_LOG("UUID handle = %llu class index = %d\n", static_cast<uint64_t>(vmBind->uuids[index]), static_cast<int>(clientHandleToConnection[vmBind->client_handle]->uuidMap[vmBind->uuids[index]].classIndex));

        auto classUuid = connection->uuidMap[uuid].classHandle;

        if (connection->classHandleToIndex.find(classUuid) != connection->classHandleToIndex.end()) {

            std::lock_guard<std::mutex> lock(asyncThreadMutex);

            if (connection->classHandleToIndex[classUuid].second ==
                static_cast<uint32_t>(NEO::DrmResourceClass::sbaTrackingBuffer)) {
                connection->vmToStateBaseAreaBindInfo[vmHandle] = {vmBind->va_start, vmBind->va_length};
            }

            if (connection->classHandleToIndex[classUuid].second ==
                static_cast<uint32_t>(NEO::DrmResourceClass::moduleHeapDebugArea)) {
                auto &isaMap = connection->isaMap[tileIndex];
                {
                    auto isa = std::make_unique<IsaAllocation>();
                    isa->bindInfo = {vmBind->va_start, vmBind->va_length};
                    isa->vmHandle = vmHandle;
                    isa->elfHandle = invalidHandle;
                    isa->moduleBegin = 0;
                    isa->moduleEnd = 0;
                    isa->tileInstanced = !(this->debugArea.isShared);
                    isa->perKernelModule = false;
                    uint32_t deviceBitfield = 0;
                    auto &debugModule = connection->uuidToModule[uuid];
                    memcpy_s(&deviceBitfield, sizeof(uint32_t),
                             &debugModule.deviceBitfield,
                             sizeof(debugModule.deviceBitfield));
                    const NEO::DeviceBitfield devices(deviceBitfield);
                    isa->deviceBitfield = devices;
                    isaMap[vmBind->va_start] = std::move(isa);
                    connection->vmToModuleDebugAreaBindInfo[vmHandle] = {vmBind->va_start,
                                                                         vmBind->va_length};
                }
            }

            if (connection->classHandleToIndex[classUuid].second ==
                static_cast<uint32_t>(NEO::DrmResourceClass::contextSaveArea)) {
                connection->vmToContextStateSaveAreaBindInfo[vmHandle] = {vmBind->va_start, vmBind->va_length};
            }
        }

        bool handleEvent = isTileWithinDeviceBitfield(tileIndex);

        if (handleEvent && connection->uuidMap[uuid].classIndex == NEO::DrmResourceClass::isa) {

            uint32_t deviceBitfield = 0;
            memcpy_s(&deviceBitfield, sizeof(uint32_t), connection->uuidMap[uuid].data.get(), connection->uuidMap[uuid].dataSize);
            const NEO::DeviceBitfield devices(deviceBitfield);

            PRINT_DEBUGGER_INFO_LOG("ISA vm_handle = %llu, tileIndex = %lu, deviceBitfield = %llu", vmHandle, tileIndex, devices.to_ulong());

            const auto isaUuidHandle = connection->uuidMap[uuid].handle;
            bool perKernelModules = true;
            int moduleUUIDindex = -1;
            bool tileInstanced = false;
            bool allInstancesEventsReceived = true;

            for (uint32_t uuidIter = 1; uuidIter < vmBind->num_uuids; uuidIter++) {
                if (connection->uuidMap[vmBind->uuids[uuidIter]].classIndex == NEO::DrmResourceClass::l0ZebinModule) {
                    perKernelModules = false;
                    moduleUUIDindex = static_cast<int>(uuidIter);
                    PRINT_DEBUGGER_INFO_LOG("Zebin module uuid = %ull", (uint64_t)vmBind->uuids[uuidIter]);
                }

                if (connection->uuidMap[vmBind->uuids[uuidIter]].classHandle == isaUuidHandle) {
                    tileInstanced = true;
                }
            }

            if (connection->isaMap[tileIndex].find(vmBind->va_start) == connection->isaMap[tileIndex].end() && createEvent) {

                auto &isaMap = connection->isaMap[tileIndex];
                auto &elfMap = connection->elfMap;

                auto isa = std::make_unique<IsaAllocation>();
                isa->bindInfo = {vmBind->va_start, vmBind->va_length};
                isa->vmHandle = vmHandle;
                isa->elfHandle = invalidHandle;
                isa->moduleBegin = 0;
                isa->moduleEnd = 0;
                isa->tileInstanced = tileInstanced;
                isa->perKernelModule = perKernelModules;
                isa->deviceBitfield = devices;

                for (index = 1; index < vmBind->num_uuids; index++) {
                    if (connection->uuidMap[vmBind->uuids[index]].classIndex == NEO::DrmResourceClass::elf) {
                        isa->elfHandle = vmBind->uuids[index];

                        if (!perKernelModules) {
                            auto &module = connection->uuidToModule[vmBind->uuids[moduleUUIDindex]];
                            DEBUG_BREAK_IF(module.elfHandle != 0 && connection->uuidMap[vmBind->uuids[index]].ptr != connection->uuidMap[module.elfHandle].ptr);

                            module.elfHandle = vmBind->uuids[index];
                            module.deviceBitfield = devices;
                        }
                    }
                }

                if (isa->elfHandle != invalidHandle) {
                    isa->moduleBegin = connection->uuidMap[isa->elfHandle].ptr;
                    isa->moduleEnd = isa->moduleBegin + connection->uuidMap[isa->elfHandle].dataSize;
                    elfMap[isa->moduleBegin] = isa->elfHandle;
                } else {
                    PRINT_DEBUGGER_ERROR_LOG("No ELF provided by application\n", "");
                }

                auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                auto loadAddress = gmmHelper->canonize(vmBind->va_start);

                zet_debug_event_t debugEvent = {};
                debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
                debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                debugEvent.info.module.load = loadAddress;
                debugEvent.info.module.moduleBegin = isa->moduleBegin;
                debugEvent.info.module.moduleEnd = isa->moduleEnd;

                std::unique_lock<std::mutex> memLock(asyncThreadMutex);
                isaMap[vmBind->va_start] = std::move(isa);

                // Expect non canonical va_start
                DEBUG_BREAK_IF(gmmHelper->decanonize(vmBind->va_start) != vmBind->va_start);

                bool apiEventNeedsAck = (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK);
                // If ACK flag is not set when triggering MODULE LOAD event, auto-ack immediately
                if (apiEventNeedsAck == false) {
                    isaMap[vmBind->va_start]->moduleLoadEventAck = true;
                }

                if (tileSessionsEnabled) {
                    auto tileAttached = tileSessions[tileIndex].second;

                    if (!tileAttached) {
                        isaMap[vmBind->va_start]->moduleLoadEventAck = true;
                        apiEventNeedsAck = false;
                    }

                    PRINT_DEBUGGER_INFO_LOG("TileDebugSession attached = %d, tileIndex = %lu, apiEventNeedsAck = %d", (int)tileAttached, tileIndex, (int)apiEventNeedsAck);
                }
                memLock.unlock();

                if (perKernelModules) {
                    PRINT_DEBUGGER_INFO_LOG("New per-kernel module\n", "");
                    debugEvent.flags = apiEventNeedsAck ? ZET_DEBUG_EVENT_FLAG_NEED_ACK : 0;

                    if (tileSessionsEnabled) {
                        auto tileAttached = static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->insertModule(debugEvent.info.module);
                        if (tileAttached) {
                            static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent);
                        }
                    } else {

                        if (devices.count() > 1) {
                            allInstancesEventsReceived = checkAllOtherTileIsaAllocationsPresent(tileIndex, vmBind->va_start);
                        }
                        if (allInstancesEventsReceived) {
                            pushApiEvent(debugEvent);
                        }
                    }
                }
            }

            if (createEvent) {
                std::lock_guard<std::mutex> lock(asyncThreadMutex);

                if (!connection->isaMap[tileIndex][vmBind->va_start]->moduleLoadEventAck && perKernelModules) {
                    bool doNotAutoAckEvent = (!blockOnFenceMode && allInstancesEventsReceived); // in block on CPU mode - do not auto-ack last event for isa instance
                    doNotAutoAckEvent |= blockOnFenceMode;                                      // in block on fence mode - do not auto-ack any events

                    if (doNotAutoAckEvent) {
                        PRINT_DEBUGGER_INFO_LOG("Add event to ack, seqno = %llu", (uint64_t)vmBind->base.seqno);
                        EventToAck ackEvent(vmBind->base.seqno, vmBind->base.type);
                        connection->isaMap[tileIndex][vmBind->va_start]->ackEvents.push_back(ackEvent);
                        shouldAckEvent = false;
                    }
                }

                connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter++;
            }

            if (destroyEvent && connection->isaMap[tileIndex].find(vmBind->va_start) != connection->isaMap[tileIndex].end()) {
                DEBUG_BREAK_IF(connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter == 0);
                connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter--;
                if (connection->isaMap[tileIndex][vmBind->va_start]->vmBindCounter == 0) {
                    const auto &isa = connection->isaMap[tileIndex][vmBind->va_start];

                    zet_debug_event_t debugEvent = {};

                    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                    auto loadAddress = gmmHelper->canonize(isa->bindInfo.gpuVa);

                    debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD;
                    debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                    debugEvent.info.module.load = loadAddress;
                    debugEvent.info.module.moduleBegin = isa->moduleBegin;
                    debugEvent.info.module.moduleEnd = isa->moduleEnd;

                    if (perKernelModules) {
                        if (tileSessionsEnabled) {
                            static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->removeModule(debugEvent.info.module);
                            static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent);
                        } else {
                            bool notifyEvent = true;
                            if (isa->deviceBitfield.count() > 1) {
                                notifyEvent = checkAllOtherTileIsaAllocationsRemoved(tileIndex, vmBind->va_start);
                            }
                            if (notifyEvent) {
                                pushApiEvent(debugEvent);
                            }
                        }
                    }
                    std::unique_lock<std::mutex> memLock(asyncThreadMutex);
                    connection->isaMap[tileIndex].erase(vmBind->va_start);
                    memLock.unlock();
                }
            }
        }

        if (handleEvent) {

            for (uint32_t uuidIter = 0; uuidIter < vmBind->num_uuids; uuidIter++) {
                if (connection->uuidMap[vmBind->uuids[uuidIter]].classIndex == NEO::DrmResourceClass::l0ZebinModule) {
                    uint64_t loadAddress = 0;
                    auto &module = connection->uuidToModule[vmBind->uuids[uuidIter]];
                    auto moduleUsedOnTile = module.deviceBitfield.test(tileIndex) || module.deviceBitfield.count() == 0;
                    if (moduleUsedOnTile) {
                        if (createEvent) {
                            module.segmentVmBindCounter[tileIndex]++;

                            DEBUG_BREAK_IF(module.loadAddresses[tileIndex].size() > module.segmentCount);
                            bool canTriggerEvent = module.loadAddresses[tileIndex].size() == (module.segmentCount - 1);
                            module.loadAddresses[tileIndex].insert(vmBind->va_start);

                            if (!blockOnFenceMode) {
                                if (canTriggerEvent && module.loadAddresses[tileIndex].size() == module.segmentCount) {
                                    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                                    loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
                                    PRINT_DEBUGGER_INFO_LOG("Zebin module loaded at: %p, with %u isa allocations", (void *)loadAddress, module.segmentCount);

                                    zet_debug_event_t debugEvent = {};
                                    debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
                                    debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                                    debugEvent.info.module.load = loadAddress;
                                    debugEvent.info.module.moduleBegin = connection->uuidMap[module.elfHandle].ptr;
                                    debugEvent.info.module.moduleEnd = connection->uuidMap[module.elfHandle].ptr + connection->uuidMap[module.elfHandle].dataSize;

                                    if (!tileSessionsEnabled) {
                                        bool allInstancesEventsReceived = true;
                                        if (module.deviceBitfield.count() > 1) {
                                            allInstancesEventsReceived = checkAllOtherTileModuleSegmentsPresent(tileIndex, module);
                                        }
                                        if (allInstancesEventsReceived) {
                                            if (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                                                debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;
                                                EventToAck ackEvent(vmBind->base.seqno, vmBind->base.type);
                                                module.ackEvents[tileIndex].push_back(ackEvent);
                                            }
                                            pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                            shouldAckEvent = false;
                                        }
                                    } else {
                                        auto tileAttached = static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->insertModule(debugEvent.info.module);

                                        if (tileAttached) {
                                            if (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                                                debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;
                                                EventToAck ackEvent(vmBind->base.seqno, vmBind->base.type);
                                                module.ackEvents[tileIndex].push_back(ackEvent);
                                            }
                                            static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                            shouldAckEvent = false;
                                        }
                                    }
                                }
                            } else {
                                PRINT_DEBUGGER_INFO_LOG("Zebin module = %ull has load addresses = %d", static_cast<uint64_t>(vmBind->uuids[uuidIter]), static_cast<int>(module.loadAddresses[tileIndex].size()));

                                if (canTriggerEvent && module.loadAddresses[tileIndex].size() == module.segmentCount) {
                                    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                                    loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
                                    PRINT_DEBUGGER_INFO_LOG("Zebin module loaded at: %p, with %u isa allocations", (void *)loadAddress, module.segmentCount);

                                    zet_debug_event_t debugEvent = {};
                                    debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
                                    debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                                    debugEvent.info.module.load = loadAddress;
                                    debugEvent.info.module.moduleBegin = connection->uuidMap[module.elfHandle].ptr;
                                    debugEvent.info.module.moduleEnd = connection->uuidMap[module.elfHandle].ptr + connection->uuidMap[module.elfHandle].dataSize;
                                    if (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK) {
                                        debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;
                                    }

                                    if (!tileSessionsEnabled) {
                                        bool allInstancesEventsReceived = true;
                                        if (module.deviceBitfield.count() > 1) {
                                            allInstancesEventsReceived = checkAllOtherTileModuleSegmentsPresent(tileIndex, module);
                                        }
                                        if (allInstancesEventsReceived) {
                                            pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                            shouldAckEvent = false;
                                        }
                                    } else {
                                        auto tileAttached = static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->insertModule(debugEvent.info.module);
                                        if (tileAttached) {
                                            static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                            shouldAckEvent = false;
                                        }
                                    }
                                }
                                {
                                    std::lock_guard<std::mutex> lock(asyncThreadMutex);
                                    if (!module.moduleLoadEventAcked[tileIndex]) {
                                        shouldAckEvent = false;
                                    }

                                    if (tileSessionsEnabled && !static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->isAttached) {
                                        shouldAckEvent = true;
                                    }
                                    if (!shouldAckEvent && (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK)) {
                                        EventToAck ackEvent(vmBind->base.seqno, vmBind->base.type);
                                        module.ackEvents[tileIndex].push_back(ackEvent);
                                    }
                                }
                            }

                        } else { // destroyEvent

                            module.segmentVmBindCounter[tileIndex]--;

                            if (module.segmentVmBindCounter[tileIndex] == 0) {

                                zet_debug_event_t debugEvent = {};

                                auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
                                auto loadAddress = gmmHelper->canonize(*std::min_element(module.loadAddresses[tileIndex].begin(), module.loadAddresses[tileIndex].end()));
                                debugEvent.type = ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD;
                                debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
                                debugEvent.info.module.load = loadAddress;
                                debugEvent.info.module.moduleBegin = connection->uuidMap[module.elfHandle].ptr;
                                debugEvent.info.module.moduleEnd = connection->uuidMap[module.elfHandle].ptr + connection->uuidMap[module.elfHandle].dataSize;

                                if (tileSessionsEnabled) {

                                    auto tileAttached = static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->removeModule(debugEvent.info.module);

                                    if (tileAttached) {
                                        static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                    }

                                } else {
                                    bool notifyEvent = true;
                                    if (module.deviceBitfield.count() > 1) {
                                        notifyEvent = checkAllOtherTileModuleSegmentsRemoved(tileIndex, module);
                                    }
                                    if (notifyEvent) {
                                        pushApiEvent(debugEvent, vmBind->uuids[uuidIter]);
                                    }
                                }
                                module.loadAddresses[tileIndex].clear();
                                module.moduleLoadEventAcked[tileIndex] = false;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    if (shouldAckEvent && (vmBind->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK)) {
        prelim_drm_i915_debug_event_ack eventToAck = {};
        eventToAck.type = vmBind->base.type;
        eventToAck.seqno = vmBind->base.seqno;
        eventToAck.flags = 0;
        auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu ret = %d errno = %d\n", (uint64_t)eventToAck.seqno, ret, ret != 0 ? errno : 0);
    }
    return true;
}

void DebugSessionLinuxi915::handleContextParamEvent(prelim_drm_i915_debug_event_context_param *contextParam) {

    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_READ_EVENT type: PRELIM_DRM_I915_DEBUG_EVENT_CONTEXT_PARAM flags = %d size = %llu client_handle = %llu ctx_handle = %llu\n",
                            (int)contextParam->base.flags, (uint64_t)contextParam->base.size, (uint64_t)contextParam->client_handle, (uint64_t)contextParam->ctx_handle);

    if (clientHandleToConnection[contextParam->client_handle]->contextsCreated.find(contextParam->ctx_handle) ==
        clientHandleToConnection[contextParam->client_handle]->contextsCreated.end()) {
        PRINT_DEBUGGER_ERROR_LOG("CONTEXT handle does not exist\n", "");
        return;
    }

    switch (contextParam->param.param) {
    case I915_CONTEXT_PARAM_VM:
        PRINT_DEBUGGER_INFO_LOG("I915_CONTEXT_PARAM_VM vm = %llu\n", (uint64_t)contextParam->param.value);
        clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].vm = contextParam->param.value;
        break;
    case I915_CONTEXT_PARAM_ENGINES: {
        PRINT_DEBUGGER_INFO_LOG("I915_CONTEXT_PARAM_ENGINES ctx_id = %lu param = %llu value = %llu size = %lu",
                                (uint32_t)contextParam->param.ctx_id,
                                (uint64_t)contextParam->param.param,
                                (uint64_t)contextParam->param.value, (uint32_t)contextParam->param.size);

        auto numEngines = (contextParam->param.size - sizeof(i915_context_param_engines)) / sizeof(i915_engine_class_instance);
        auto engines = reinterpret_cast<i915_context_param_engines *>(&(contextParam->param.value));

        clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].engines.clear();

        for (uint32_t i = 0; i < numEngines; i++) {
            clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].engines.push_back(engines->engines[i]);
        }

        auto vm = clientHandleToConnection[contextParam->client_handle]->contextsCreated[contextParam->ctx_handle].vm;
        if (numEngines && vm != invalidHandle) {
            NEO::EngineClassInstance engineClassInstance = {engines->engines[0].engine_class, engines->engines[0].engine_instance};
            auto tileIndex = DrmHelper::getEngineTileIndex(connectedDevice, engineClassInstance);
            clientHandleToConnection[contextParam->client_handle]->vmToTile[vm] = tileIndex;

            PRINT_DEBUGGER_INFO_LOG("VM = %llu mapped to TILE = %lu\n", vm, tileIndex);
        }

        break;
    }
    default:
        PRINT_DEBUGGER_INFO_LOG("I915_CONTEXT_PARAM UNHANDLED = %llu\n", (uint64_t)contextParam->param.param);
        break;
    }
}

uint64_t DebugSessionLinuxi915::getVmHandleFromClientAndlrcHandle(uint64_t clientHandle, uint64_t lrcHandle) {

    if (clientHandleToConnection.find(clientHandle) == clientHandleToConnection.end()) {
        return invalidHandle;
    }

    auto &clientConnection = clientHandleToConnection[clientHandle];
    if (clientConnection->lrcToContextHandle.find(lrcHandle) == clientConnection->lrcToContextHandle.end()) {
        return invalidHandle;
    }

    auto contextHandle = clientConnection->lrcToContextHandle[lrcHandle];
    if (clientConnection->contextsCreated.find(contextHandle) == clientConnection->contextsCreated.end()) {
        return invalidHandle;
    }

    return clientConnection->contextsCreated[contextHandle].vm;
}

void DebugSessionLinuxi915::handleAttentionEvent(prelim_drm_i915_debug_event_eu_attention *attention) {
    NEO::EngineClassInstance engineClassInstance = {attention->ci.engine_class, attention->ci.engine_instance};
    auto tileIndex = DrmHelper::getEngineTileIndex(connectedDevice, engineClassInstance);
    auto tmpInterruptSent = tileSessionsEnabled ? tileSessions[tileIndex].first->isInterruptSent() : interruptSent.load();
    if (tmpInterruptSent && attention->base.seqno <= euControlInterruptSeqno[tileIndex]) {
        PRINT_DEBUGGER_INFO_LOG("Discarding EU ATTENTION event for interrupt request. Event seqno == %d <= %d == interrupt seqno\n",
                                (uint32_t)attention->base.seqno,
                                (uint32_t)euControlInterruptSeqno[tileIndex]);
        return;
    }

    newAttentionRaised();
    if (!connectedDevice->getNEODevice()->getDeviceBitfield().test(tileIndex)) {
        return;
    }

    std::vector<EuThread::ThreadId> threadsWithAttention;
    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();
    if (tmpInterruptSent) {
        std::unique_ptr<uint8_t[]> bitmask;
        size_t bitmaskSize;
        auto attReadResult = threadControl({}, tileIndex, ThreadControlCmd::stopped, bitmask, bitmaskSize);
        if (attReadResult == 0) {
            threadsWithAttention = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, tileIndex, bitmask.get(), bitmaskSize);
        }
    }

    AttentionEventFields attentionEventFields;
    attentionEventFields.bitmask = attention->bitmask;
    attentionEventFields.bitmaskSize = attention->bitmask_size;
    attentionEventFields.clientHandle = attention->client_handle;
    attentionEventFields.contextHandle = attention->ctx_handle;
    attentionEventFields.lrcHandle = attention->lrc_handle;

    if (updateStoppedThreadsAndCheckTriggerEvents(attentionEventFields, tileIndex, threadsWithAttention) != ZE_RESULT_SUCCESS) {
        PRINT_DEBUGGER_ERROR_LOG("Failed to update stopped threads and check trigger events\n", "");
    }
}

std::unique_lock<std::mutex> DebugSessionLinuxi915::getThreadStateMutexForTileSession(uint32_t tileIndex) {
    return std::unique_lock<std::mutex>(static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->threadStateMutex);
}
void DebugSessionLinuxi915::checkTriggerEventsForAttentionForTileSession(uint32_t tileIndex) {
    static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->checkTriggerEventsForAttention();
}
void DebugSessionLinuxi915::addThreadToNewlyStoppedFromRaisedAttentionForTileSession(EuThread::ThreadId threadId,
                                                                                     uint64_t memoryHandle,
                                                                                     const void *stateSaveArea,
                                                                                     uint32_t tileIndex) {
    static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->addThreadToNewlyStoppedFromRaisedAttention(threadId, memoryHandle, stateSaveAreaMemory.data());
}

void DebugSessionLinuxi915::handleEnginesEvent(prelim_drm_i915_debug_event_engines *engines) {
    PRINT_DEBUGGER_INFO_LOG("ENGINES event: client_handle = %llu, ctx_handle = %llu, num_engines = %llu %s\n",
                            (uint64_t)engines->client_handle,
                            (uint64_t)engines->ctx_handle,
                            (uint64_t)engines->num_engines,
                            engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE ? "CREATE" : engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY ? "DESTROY"
                                                                                                                                                            : "");

    UNRECOVERABLE_IF(clientHandleToConnection.find(engines->client_handle) == clientHandleToConnection.end());

    if (engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_CREATE) {
        for (uint64_t i = 0; i < engines->num_engines; ++i) {
            auto lrc = engines->engines[i].lrc_handle;
            if (lrc != 0) {
                PRINT_DEBUGGER_INFO_LOG("    lrc%llu = %llu", i, lrc);
            }
            clientHandleToConnection[engines->client_handle]->lrcToContextHandle[lrc] = engines->ctx_handle;
        }
    }

    if (engines->base.flags & PRELIM_DRM_I915_DEBUG_EVENT_DESTROY) {
        for (uint64_t i = 0; i < engines->num_engines; ++i) {
            auto lrc = engines->engines[i].lrc_handle;
            PRINT_DEBUGGER_INFO_LOG("    lrc%llu = %llu\n", i, lrc);
            clientHandleToConnection[engines->client_handle]->lrcToContextHandle.erase(lrc);
        }
    }
}

void DebugSessionLinuxi915::pushApiEventForTileSession(uint32_t tileIndex, zet_debug_event_t &debugEvent) {
    static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->pushApiEvent(debugEvent);
}

void DebugSessionLinuxi915::setPageFaultForTileSession(uint32_t tileIndex, EuThread::ThreadId threadId, bool hasPageFault) {
    static_cast<TileDebugSessionLinuxi915 *>(tileSessions[tileIndex].first)->allThreads[threadId]->setPageFault(true);
}

void DebugSessionLinuxi915::extractUuidData(uint64_t client, const UuidData &uuidData) {
    if (uuidData.classIndex == NEO::DrmResourceClass::sbaTrackingBuffer ||
        uuidData.classIndex == NEO::DrmResourceClass::moduleHeapDebugArea ||
        uuidData.classIndex == NEO::DrmResourceClass::contextSaveArea) {
        UNRECOVERABLE_IF(uuidData.dataSize != 8);
        uint64_t *data = (uint64_t *)uuidData.data.get();

        if (uuidData.classIndex == NEO::DrmResourceClass::sbaTrackingBuffer) {
            clientHandleToConnection[client]->stateBaseAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("SbaTrackingBuffer GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->stateBaseAreaGpuVa);
        }
        if (uuidData.classIndex == NEO::DrmResourceClass::moduleHeapDebugArea) {
            clientHandleToConnection[client]->moduleDebugAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ModuleHeapDebugArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->moduleDebugAreaGpuVa);
        }
        if (uuidData.classIndex == NEO::DrmResourceClass::contextSaveArea) {
            clientHandleToConnection[client]->contextStateSaveAreaGpuVa = *data;
            PRINT_DEBUGGER_INFO_LOG("ContextSaveArea GPU VA = %p", (void *)clientHandleToConnection[clientHandle]->contextStateSaveAreaGpuVa);
        }
    }

    if (uuidData.classIndex == NEO::DrmResourceClass::l0ZebinModule) {
        uint32_t segmentCount = 0;
        memcpy_s(&segmentCount, sizeof(uint32_t), uuidData.data.get(), uuidData.dataSize);
        clientHandleToConnection[client]->uuidToModule[uuidData.handle].segmentCount = segmentCount;
        PRINT_DEBUGGER_INFO_LOG("Zebin module = %ull, segment count = %ul", uuidData.handle, segmentCount);
    }
}

uint64_t DebugSessionLinuxi915::extractVaFromUuidString(std::string &uuid) {
    const char uuidString[] = "%04" SCNx64 "-%012" SCNx64;
    auto subString = uuid.substr(19);

    uint64_t parts[2] = {0, 0};
    sscanf(subString.c_str(), uuidString, &parts[1], &parts[0]);

    parts[0] |= (parts[1] & 0xFFFF) << 48;
    return parts[0];
}

int DebugSessionLinuxi915::threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmaskOut, size_t &bitmaskSizeOut) {

    auto hwInfo = connectedDevice->getHwInfo();
    auto classInstance = DrmHelper::getEngineInstance(connectedDevice, tile, hwInfo.capabilityTable.defaultEngineType);
    UNRECOVERABLE_IF(!classInstance);

    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();

    bitmaskSizeOut = 0;

    struct prelim_drm_i915_debug_eu_control euControl = {};
    euControl.client_handle = clientHandle;
    euControl.ci.engine_class = classInstance->engineClass;
    euControl.ci.engine_instance = classInstance->engineInstance;
    euControl.bitmask_size = 0;
    euControl.bitmask_ptr = 0;
    decltype(prelim_drm_i915_debug_eu_control::cmd) command = 0;
    switch (threadCmd) {
    case ThreadControlCmd::interruptAll:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT_ALL;
        break;
    case ThreadControlCmd::interrupt:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT;
        break;
    case ThreadControlCmd::resume:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME;
        break;
    case ThreadControlCmd::stopped:
        command = PRELIM_I915_DEBUG_EU_THREADS_CMD_STOPPED;
        break;
    }
    euControl.cmd = command;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;

    if (command == PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT ||
        command == PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME ||
        command == PRELIM_I915_DEBUG_EU_THREADS_CMD_STOPPED) {
        l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);
        euControl.bitmask_size = static_cast<uint32_t>(bitmaskSize);
        euControl.bitmask_ptr = reinterpret_cast<uint64_t>(bitmask.get());
    }

    if (command == PRELIM_I915_DEBUG_EU_THREADS_CMD_RESUME) {
        applyResumeWa(bitmask.get(), bitmaskSize);
    }

    printBitmask(bitmask.get(), bitmaskSize);

    auto euControlRetVal = ioctl(PRELIM_I915_DEBUG_IOCTL_EU_CONTROL, &euControl);
    if (euControlRetVal != 0) {
        PRINT_DEBUGGER_ERROR_LOG("PRELIM_I915_DEBUG_IOCTL_EU_CONTROL failed: retCode: %d errno = %d command = %d\n", euControlRetVal, errno, command);
    } else {
        PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_EU_CONTROL: seqno = %llu command = %u\n", (uint64_t)euControl.seqno, command);
    }

    if (command == PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT ||
        command == PRELIM_I915_DEBUG_EU_THREADS_CMD_INTERRUPT_ALL) {
        if (euControlRetVal == 0) {
            euControlInterruptSeqno[tile] = euControl.seqno;
        } else {
            euControlInterruptSeqno[tile] = invalidHandle;
        }
    }

    if (threadCmd == ThreadControlCmd::stopped) {
        bitmaskOut = std::move(bitmask);
        UNRECOVERABLE_IF(bitmaskOut.get() == nullptr);
        bitmaskSizeOut = euControl.bitmask_size;
    }
    return euControlRetVal;
}

void DebugSessionLinuxi915::printContextVms() {
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO) {
        PRINT_DEBUGGER_LOG(stdout, "\nINFO: Context - VM map: ", "");
        for (size_t i = 0; i < clientHandleToConnection[clientHandle]->contextsCreated.size(); i++) {
            PRINT_DEBUGGER_LOG(stdout, "\n Context = %llu : %llu ", (uint64_t)clientHandleToConnection[clientHandle]->contextsCreated[i].handle,
                               (uint64_t)clientHandleToConnection[clientHandle]->contextsCreated[i].vm);
        }
    }
}

int DebugSessionLinuxi915::eventAckIoctl(EventToAck &event) {
    prelim_drm_i915_debug_event_ack eventToAck = {};
    eventToAck.type = event.type;
    eventToAck.seqno = event.seqno;
    eventToAck.flags = 0;

    auto ret = ioctl(PRELIM_I915_DEBUG_IOCTL_ACK_EVENT, &eventToAck);
    PRINT_DEBUGGER_INFO_LOG("PRELIM_I915_DEBUG_IOCTL_ACK_EVENT seqno = %llu, ret = %d errno = %d\n", (uint64_t)event.seqno, ret, ret != 0 ? errno : 0);
    return ret;
}

void TileDebugSessionLinuxi915::readStateSaveAreaHeader() {

    const auto header = rootDebugSession->getStateSaveAreaHeader();
    if (header) {
        auto headerSize = rootDebugSession->stateSaveAreaHeader.size();
        this->stateSaveAreaHeader.assign(reinterpret_cast<const char *>(header), reinterpret_cast<const char *>(header) + headerSize);
        this->sipSupportsSlm = rootDebugSession->sipSupportsSlm;
    }
};

bool TileDebugSessionLinuxi915::insertModule(zet_debug_event_info_module_t module) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    modules.insert({module.load, module});
    return isAttached;
}

bool TileDebugSessionLinuxi915::removeModule(zet_debug_event_info_module_t module) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    modules.erase(module.load);
    return isAttached;
}

bool TileDebugSessionLinuxi915::processEntry() {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    processEntryState = true;
    return isAttached;
}

bool TileDebugSessionLinuxi915::processExit() {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    processEntryState = false;
    return isAttached;
}

void TileDebugSessionLinuxi915::attachTile() {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);

    // clear apiEvents queue
    apiEvents = decltype(apiEvents){};

    if (detached) {
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_DETACHED;
        debugEvent.info.detached.reason = ZET_DEBUG_DETACH_REASON_INVALID;

        apiEvents.push(debugEvent);
    } else {
        if (processEntryState) {
            zet_debug_event_t event = {};
            event.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;
            event.flags = 0;
            apiEvents.push(event);
        }

        if (modules.size()) {
            zet_debug_event_t event = {};
            event.type = ZET_DEBUG_EVENT_TYPE_MODULE_LOAD;
            event.flags = 0;

            for (const auto &module : modules) {
                memcpy_s(&event.info.module, sizeof(event.info.module), &module.second, sizeof(module.second));
                apiEvents.push(event);
            }
        }
    }
    isAttached = true;
}

void TileDebugSessionLinuxi915::detachTile() {
    std::vector<uint64_t> moduleUuids;
    {
        std::lock_guard<std::mutex> lock(asyncThreadMutex);
        for (const auto &eventToAck : eventsToAck) {
            auto moduleUUID = eventToAck.second;
            moduleUuids.push_back(moduleUUID);
        }
        eventsToAck.clear();
        isAttached = false;
    }

    for (const auto &uuid : moduleUuids) {
        rootDebugSession->ackModuleEvents(this->tileIndex, uuid);
    }
}

} // namespace L0
