/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

#include "platforms.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_ROCKETLAKE;

#include "shared/source/gen12lp/os_agnostic_hw_info_config_gen12lp.inl"
#include "shared/source/gen12lp/rkl/os_agnostic_hw_info_config_rkl.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;
    hwInfo->featureTable.flags.ftrGpGpuMidThreadLevelPreempt = false;

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
