/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_ROCKETLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;
    hwInfo->featureTable.ftrGpGpuMidThreadLevelPreempt = false;

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template class HwInfoConfigHw<IGFX_ROCKETLAKE>;
} // namespace NEO
