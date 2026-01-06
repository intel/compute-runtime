/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"

namespace NEO {
template <>
uint32_t ProductHelperHw<gfxProduct>::getActualHwSlmSize(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return hwInfo.gtSystemInfo.SLMSizeInKb;
}
} // namespace NEO