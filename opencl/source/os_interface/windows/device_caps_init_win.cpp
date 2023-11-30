/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef _WIN32

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"

#include "opencl/source/cl_device/cl_device.h"

namespace NEO {

void ClDevice::initializeOsSpecificCaps() {
    if (enabledClVersion >= 30 && debugManager.flags.ClKhrExternalMemoryExtension.get()) {
        deviceInfo.externalMemorySharing = CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR;
    }

    deviceExtensions += "cl_intel_simultaneous_sharing ";
    deviceInfo.deviceExtensions = deviceExtensions.c_str();

    simultaneousInterops = {CL_GL_CONTEXT_KHR,
                            CL_WGL_HDC_KHR,
                            CL_CONTEXT_ADAPTER_D3D9_KHR,
                            CL_CONTEXT_D3D9_DEVICE_INTEL,
                            CL_CONTEXT_ADAPTER_D3D9EX_KHR,
                            CL_CONTEXT_D3D9EX_DEVICE_INTEL,
                            CL_CONTEXT_ADAPTER_DXVA_KHR,
                            CL_CONTEXT_DXVA_DEVICE_INTEL,
                            CL_CONTEXT_D3D10_DEVICE_KHR,
                            CL_CONTEXT_D3D11_DEVICE_KHR,
                            0};
}

} // namespace NEO

#endif
