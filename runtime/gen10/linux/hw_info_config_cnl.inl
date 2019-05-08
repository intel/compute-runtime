/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_CANNONLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    if (gtSystemInfo->SubSliceCount == 9) {
        gtSystemInfo->SliceCount = 4;
    } else if (gtSystemInfo->SubSliceCount > 5) {
        gtSystemInfo->SliceCount = 3;
    } else if (gtSystemInfo->SubSliceCount > 3) {
        gtSystemInfo->SliceCount = 2;
    } else {
        gtSystemInfo->SliceCount = 1;
    }

    if ((gtSystemInfo->SliceCount > 1) && (gtSystemInfo->IsL3HashModeEnabled)) {
        gtSystemInfo->L3BankCount--;
        gtSystemInfo->L3CacheSizeInKb -= 256;
    }
    return 0;
}

template class HwInfoConfigHw<IGFX_CANNONLAKE>;
} // namespace NEO
