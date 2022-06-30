/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_ALDERLAKE_S>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }

    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    hwInfo->featureTable.flags.ftrGpGpuMidThreadLevelPreempt = (hwInfo->platform.usRevId >= hwHelper.getHwRevIdFromStepping(REVISION_C, *hwInfo));
    return 0;
}

template class HwInfoConfigHw<IGFX_ALDERLAKE_S>;
} // namespace NEO
