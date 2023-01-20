/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"

namespace NEO {
std::unique_ptr<GLSharingFunctions> GLSharingFunctions::create() {
    return std::make_unique<GLSharingFunctionsWindows>();
}

bool GLSharingFunctionsWindows::isHandleCompatible(const DriverModel &driverModel, uint32_t handle) const {

    return driverModel.as<const Wddm>()->verifyAdapterLuid(getAdapterLuid(reinterpret_cast<GLContext>(static_cast<uintptr_t>(handle))));
}

} // namespace NEO
