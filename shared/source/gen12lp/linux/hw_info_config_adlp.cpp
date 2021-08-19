/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/helpers_gen12lp.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_ALDERLAKE_P;

#include "shared/source/gen12lp/os_agnostic_hw_info_config_gen12lp.inl"

template <>
void HwInfoConfigHw<IGFX_ALDERLAKE_P>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    PLATFORM *platform = &hwInfo->platform;
    platform->eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform->eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

template <>
int HwInfoConfigHw<IGFX_ALDERLAKE_P>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;
    HwHelper &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    hwInfo->featureTable.ftrGpGpuMidThreadLevelPreempt = (hwInfo->platform.usRevId >= hwHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo));

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template class HwInfoConfigHw<IGFX_ALDERLAKE_P>;
} // namespace NEO
