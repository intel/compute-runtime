/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/helpers_gen12lp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_ROCKETLAKE;

#include "shared/source/gen12lp/os_agnostic_hw_info_config_gen12lp.inl"

template <>
void HwInfoConfigHw<IGFX_ROCKETLAKE>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPHelpers::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}

template <>
int HwInfoConfigHw<IGFX_ROCKETLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;
    hwInfo->featureTable.ftrGpGpuMidThreadLevelPreempt = false;

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <>
uint32_t HwInfoConfigHw<IGFX_ROCKETLAKE>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_B:
        return 0x1;
    case REVISION_C:
        return 0x4;
    }
    return CommonConstants::invalidStepping;
}

template class HwInfoConfigHw<IGFX_ROCKETLAKE>;
} // namespace NEO
