/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_LAKEFIELD>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }

    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

    pSysInfo->SliceCount = 1;

    pSkuTable->ftrPPGTT = true;
    pSkuTable->ftrSVM = true;
    pSkuTable->ftrL3IACoherency = true;
    pSkuTable->ftrIA32eGfxPTEs = true;
    pSkuTable->ftrStandardMipTailFormat = true;

    pSkuTable->ftrDisplayYTiling = true;
    pSkuTable->ftrTranslationTable = true;
    pSkuTable->ftrUserModeTranslationTable = true;
    pSkuTable->ftrTileMappedResource = true;
    pSkuTable->ftrEnableGuC = true;

    pSkuTable->ftrFbc = true;
    pSkuTable->ftrFbc2AddressTranslation = true;
    pSkuTable->ftrFbcBlitterTracking = true;
    pSkuTable->ftrFbcCpuTracking = true;
    pSkuTable->ftrTileY = true;

    pSkuTable->ftrAstcHdr2D = true;
    pSkuTable->ftrAstcLdr2D = true;

    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    pWaTable->waReportPerfCountUseGlobalContextID = true;

    return 0;
}

template class HwInfoConfigHw<IGFX_LAKEFIELD>;
} // namespace NEO
