/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

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

    // closeAsyncThread();
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

} // namespace L0