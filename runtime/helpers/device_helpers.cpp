/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/device_helpers.h"

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {
void DeviceHelper::getExtraDeviceInfo(cl_device_info paramName, cl_uint &param, const void *&src, size_t &size, size_t &retSize) {}

uint32_t DeviceHelper::getDevicesCount(const HardwareInfo *pHwInfo) {
    return DebugManager.flags.CreateMultipleDevices.get() > 0 ? DebugManager.flags.CreateMultipleDevices.get() : 1u;
}
} // namespace OCLRT
