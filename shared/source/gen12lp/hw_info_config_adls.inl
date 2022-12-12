/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
int ProductHelperHw<IGFX_ALDERLAKE_S>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }

    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;
    GfxCoreHelper &gfxCoreHelper = GfxCoreHelper::get(hwInfo.platform.eRenderCoreFamily);
    hwInfo->featureTable.flags.ftrGpGpuMidThreadLevelPreempt = (hwInfo->platform.usRevId >= gfxCoreHelper.getHwRevIdFromStepping(REVISION_C, *hwInfo));
    return 0;
}

template class ProductHelperHw<IGFX_ALDERLAKE_S>;
} // namespace NEO
