/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/device_helpers.h"

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace NEO {
void DeviceHelper::getExtraDeviceInfo(const HardwareInfo &hwInfo, cl_device_info paramName, cl_uint &param, const void *&src, size_t &size, size_t &retSize) {}

uint32_t DeviceHelper::getSubDevicesCount(const HardwareInfo *pHwInfo) {
    return DebugManager.flags.CreateMultipleSubDevices.get() > 0 ? DebugManager.flags.CreateMultipleSubDevices.get() : 1u;
}

uint32_t DeviceHelper::getEnginesCount(const HardwareInfo &hwInfo) {
    return 1u;
}
} // namespace NEO
