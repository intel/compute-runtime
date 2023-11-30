/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/cl_device/cl_device.h"

namespace NEO {

void ClDevice::initializeOsSpecificCaps() {
    if (enabledClVersion >= 30 && debugManager.flags.ClKhrExternalMemoryExtension.get()) {
        deviceInfo.externalMemorySharing = CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR;
    }
}

} // namespace NEO
