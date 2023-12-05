/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session_factory.h"

namespace L0 {
DebugSessionLinuxAllocatorFn debugSessionLinuxFactory[DEBUG_SESSION_LINUX_TYPE_MAX] = {};

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {

    if (!device->getOsInterface().isDebugAttachAvailable()) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }
    auto drm = device->getOsInterface().getDriverModel()->as<NEO::Drm>();
    auto drmVersion = NEO::Drm::getDrmVersion(drm->getFileDescriptor());

    DebugSessionLinuxAllocatorFn allocator = nullptr;
    if ("xe" == drmVersion) {
        allocator = debugSessionLinuxFactory[DEBUG_SESSION_LINUX_TYPE_XE];
    } else {
        allocator = debugSessionLinuxFactory[DEBUG_SESSION_LINUX_TYPE_I915];
    }
    UNRECOVERABLE_IF(!allocator)

    auto debugSession = allocator(config, device, result, isRootAttach);
    if (result != ZE_RESULT_SUCCESS) {
        return nullptr;
    }
    debugSession->setAttachMode(isRootAttach);
    result = debugSession->initialize();
    if (result != ZE_RESULT_SUCCESS) {
        debugSession->closeConnection();
        delete debugSession;
        debugSession = nullptr;
    } else {
        debugSession->startAsyncThread();
    }
    return debugSession;
}

ze_result_t DebugSessionLinux::translateDebuggerOpenErrno(int error) {

    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    switch (error) {
    case ENODEV:
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    case EBUSY:
        result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        break;
    case EACCES:
        result = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        break;
    }
    return result;
}

bool DebugSessionLinux::closeFd() {
    if (fd == 0) {
        return false;
    }

    auto ret = NEO::SysCalls::close(fd);

    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("Debug connection close() on fd: %d failed: retCode: %d\n", fd, ret);
        return false;
    }
    fd = 0;
    return true;
}

void *DebugSessionLinux::readInternalEventsThreadFunction(void *arg) {
    DebugSessionLinux *self = reinterpret_cast<DebugSessionLinux *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger internal event thread started\n", "");
    self->internalThreadHasStarted = true;

    while (self->internalEventThread.threadActive) {
        self->readInternalEventsAsync();
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger internal event thread closing\n", "");

    return nullptr;
}

std::unique_ptr<uint64_t[]> DebugSessionLinux::getInternalEvent() {
    std::unique_ptr<uint64_t[]> eventMemory;

    {
        std::unique_lock<std::mutex> lock(internalEventThreadMutex);

        if (internalEventQueue.empty()) {
            NEO::waitOnCondition(internalEventCondition, lock, std::chrono::milliseconds(100));
        }

        if (!internalEventQueue.empty()) {
            eventMemory = std::move(internalEventQueue.front());
            internalEventQueue.pop();
        }
    }
    return eventMemory;
}

} // namespace L0
