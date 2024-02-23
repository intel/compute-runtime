/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
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

ze_result_t DebugSessionLinuxXe::initialize() {

    struct pollfd pollFd = {
        .fd = this->fd,
        .events = POLLIN,
        .revents = 0,
    };

    auto numberOfFds = ioctlHandler->poll(&pollFd, 1, 1000);
    PRINT_DEBUGGER_INFO_LOG("initialization poll() retCode: %d\n", numberOfFds);

    if (numberOfFds <= 0) {
        return ZE_RESULT_NOT_READY;
    }

    bool isRootDevice = !connectedDevice->getNEODevice()->isSubDevice();
    UNRECOVERABLE_IF(!isRootDevice);

    createEuThreads();
    tileSessionsEnabled = false;

    startInternalEventsThread();

    return ZE_RESULT_SUCCESS;
}

void DebugSessionLinuxXe::handleEventsAsync() {
    auto eventMemory = getInternalEvent();
    if (eventMemory != nullptr) {
        auto debugEvent = reinterpret_cast<drm_xe_eudebug_event *>(eventMemory.get());
        handleEvent(debugEvent);
    }
}

void *DebugSessionLinuxXe::asyncThreadFunction(void *arg) {
    DebugSessionLinuxXe *self = reinterpret_cast<DebugSessionLinuxXe *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger async thread start\n", "");

    while (self->asyncThread.threadActive) {
        self->handleEventsAsync();
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

        int maxLoopCount = 3;
        do {

            uint8_t maxEventBuffer[sizeof(drm_xe_eudebug_event) + maxEventSize];
            auto event = reinterpret_cast<drm_xe_eudebug_event *>(maxEventBuffer);
            event->len = maxEventSize;
            event->type = DRM_XE_EUDEBUG_EVENT_READ;
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

int DebugSessionLinuxXe::ioctl(unsigned long request, void *arg) {
    return ioctlHandler->ioctl(fd, request, arg);
}

ze_result_t DebugSessionLinuxXe::readEventImp(drm_xe_eudebug_event *drmDebugEvent) {
    auto ret = ioctl(DRM_XE_EUDEBUG_IOCTL_READ_EVENT, drmDebugEvent);
    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT failed: retCode: %d errno = %d\n", ret, errno);
        return ZE_RESULT_NOT_READY;
    } else if (drmDebugEvent->flags & ~static_cast<uint32_t>(DRM_XE_EUDEBUG_EVENT_CREATE | DRM_XE_EUDEBUG_EVENT_DESTROY | DRM_XE_EUDEBUG_EVENT_STATE_CHANGE)) {
        PRINT_DEBUGGER_ERROR_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT unsupported flag = %d\n", (int)drmDebugEvent->flags);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

void DebugSessionLinuxXe::pushApiEvent(zet_debug_event_t &debugEvent) {
    std::unique_lock<std::mutex> lock(asyncThreadMutex);
    apiEvents.push(debugEvent);
    apiEventCondition.notify_all();
}

bool DebugSessionLinuxXe::closeConnection() {
    closeInternalEventsThread();
    return closeFd();
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
        }

        if (event->flags & DRM_XE_EUDEBUG_EVENT_DESTROY) {
            clientHandleClosed = clientEvent->client_handle;
        }

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_OPEN flags = %u len = %lu client.handle = %llu\n",
                                (uint16_t)event->flags, (uint32_t)event->len, (uint64_t)clientEvent->client_handle);

    } break;

    case DRM_XE_EUDEBUG_EVENT_VM: {
        drm_xe_eudebug_event_vm *vm = reinterpret_cast<drm_xe_eudebug_event_vm *>(event);

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_VM flags = %u size = %lu client_handle = %llu vm_handle = %llu\n",
                                (uint16_t)event->flags, (uint32_t)event->len, (uint64_t)vm->client_handle, (uint64_t)vm->vm_handle);

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

            clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].vmHandle = execQueue->vm_handle;
            clientHandleToConnection[execQueue->client_handle]->execQueues[execQueue->exec_queue_handle].engineClass = execQueue->engine_class;
            for (uint16_t idx = 0; idx < execQueue->width; idx++) {
                clientHandleToConnection[execQueue->client_handle]->lrcHandleToVmHandle[execQueue->lrc_handle[idx]] = execQueue->vm_handle;
            }
        }

        if (event->flags & DRM_XE_EUDEBUG_EVENT_DESTROY) {
            for (uint16_t idx = 0; idx < execQueue->width; idx++) {
                clientHandleToConnection[execQueue->client_handle]->lrcHandleToVmHandle.erase(execQueue->lrc_handle[idx]);
            }
            clientHandleToConnection[execQueue->client_handle]->execQueues.erase(execQueue->exec_queue_handle);
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

        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE flags = %u len = %lu client_handle = %llu\
        vm_handle = %llu exec_queue_handle = %llu engine_class = %u\n",
                                (uint16_t)event->flags, (uint32_t)event->len, (uint64_t)execQueue->client_handle, (uint64_t)execQueue->vm_handle,
                                (uint64_t)execQueue->exec_queue_handle, (uint16_t)execQueue->engine_class);
    } break;

    default:
        PRINT_DEBUGGER_INFO_LOG("DRM_XE_EUDEBUG_IOCTL_READ_EVENT type: UNHANDLED %u flags = %u len = %lu\n", (uint16_t)event->type, (uint16_t)event->flags, (uint32_t)event->len);
        break;
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

int DebugSessionLinuxXe::euControlIoctl(ThreadControlCmd threadCmd,
                                        const NEO::EngineClassInstance *classInstance,
                                        std::unique_ptr<uint8_t[]> &bitmask,
                                        size_t bitmaskSize, uint64_t &seqnoOut, uint64_t &bitmaskSizeOut) {
    struct drm_xe_eudebug_eu_control euControl = {};
    euControl.client_handle = clientHandle;
    euControl.ci.engine_class = classInstance->engineClass;
    euControl.ci.engine_instance = classInstance->engineInstance;
    euControl.bitmask_size = 0;
    euControl.bitmask_ptr = 0;

    decltype(drm_xe_eudebug_eu_control::cmd) command = 0;
    switch (threadCmd) {
    case ThreadControlCmd::interruptAll:
        command = DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL;
        break;
    case ThreadControlCmd::resume:
        command = DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME;
        break;
    case ThreadControlCmd::stopped:
        command = DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED;
        break;
    default:
        command = 0xFFFFFFFF;
        break;
    }
    euControl.cmd = command;

    euControl.bitmask_size = static_cast<uint32_t>(bitmaskSize);
    euControl.bitmask_ptr = reinterpret_cast<uint64_t>(bitmask.get());

    printBitmask(bitmask.get(), bitmaskSize);

    auto euControlRetVal = ioctl(DRM_XE_EUDEBUG_IOCTL_EU_CONTROL, &euControl);
    seqnoOut = euControl.seqno;
    bitmaskSizeOut = euControl.bitmask_size;
    return euControlRetVal;
}

} // namespace L0