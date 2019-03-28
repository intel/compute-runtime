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
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);

    pSkuTable->ftrGpGpuMidBatchPreempt = true;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = true;
    pSkuTable->ftrL3IACoherency = true;

    if (pSysInfo->SubSliceCount == 9) {
        pSysInfo->SliceCount = 4;
    } else if (pSysInfo->SubSliceCount > 5) {
        pSysInfo->SliceCount = 3;
    } else if (pSysInfo->SubSliceCount > 3) {
        pSysInfo->SliceCount = 2;
    } else {
        pSysInfo->SliceCount = 1;
    }

    if ((pSysInfo->SliceCount > 1) && (pSysInfo->IsL3HashModeEnabled)) {
        pSysInfo->L3BankCount--;
        pSysInfo->L3CacheSizeInKb -= 256;
    }
    return 0;
}

template class HwInfoConfigHw<IGFX_CANNONLAKE>;
} // namespace NEO
