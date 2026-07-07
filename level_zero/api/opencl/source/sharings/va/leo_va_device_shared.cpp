/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_device.h"

#include <va/va_backend.h>

namespace NEO {
ClDevice *VADevice::getRootDeviceFromVaDisplay(Platform *pPlatform, VADisplay vaDisplay) {
    return nullptr;
}
} // namespace NEO
