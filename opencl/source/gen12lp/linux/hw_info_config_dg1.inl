/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_info_config_common_helper.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_DG1>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }

    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;

    HwInfoConfigCommonHelper::enableBlitterOperationsSupport(*hwInfo);
    hwInfo->featureTable.ftrGpGpuMidThreadLevelPreempt = false;
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_DG1>::getSharedSystemMemCapabilities() {
    return 0;
}

template class HwInfoConfigHw<IGFX_DG1>;
} // namespace NEO
