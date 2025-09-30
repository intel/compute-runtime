/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {
template <>
uint32_t ProductHelperHw<gfxProduct>::getActualHwSlmSize(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (rootDeviceEnvironment.isWddmOnLinux()) {
        return hwInfo.gtSystemInfo.SLMSizeInKb / hwInfo.gtSystemInfo.DualSubSliceCount;
    } else {
        return hwInfo.gtSystemInfo.SLMSizeInKb;
    }
}
} // namespace NEO