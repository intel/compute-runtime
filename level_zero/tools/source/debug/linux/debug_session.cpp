/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"

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
    if (!allocator) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }

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

} // namespace L0
