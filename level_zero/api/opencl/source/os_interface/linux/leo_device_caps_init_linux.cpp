/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"

namespace NEO {
namespace LEO {

void ClDevice::initializeOsSpecificCaps() {
    if (debugManager.flags.ClKhrExternalMemoryExtension.get()) {
        deviceInfo.externalMemorySharing = CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR;
    }
}

} // namespace LEO
} // namespace NEO
