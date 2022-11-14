/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_hw_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

ClHwHelper *clHwHelperFactory[IGFX_MAX_CORE] = {};

ClHwHelper &ClHwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *clHwHelperFactory[gfxCore];
}

uint8_t ClHwHelper::makeDeviceRevision(const HardwareInfo &hwInfo) {
    return static_cast<uint8_t>(!hwInfo.capabilityTable.isIntegratedDevice);
}

cl_version ClHwHelper::makeDeviceIpVersion(uint16_t major, uint8_t minor, uint8_t revision) {
    return (major << 16) | (minor << 8) | revision;
}

template <>
ClHwHelper &RootDeviceEnvironment::getHelper<ClHwHelper>() const {
    auto &apiHelper = ClHwHelper::get(this->getHardwareInfo()->platform.eRenderCoreFamily);
    return apiHelper;
}

} // namespace NEO
