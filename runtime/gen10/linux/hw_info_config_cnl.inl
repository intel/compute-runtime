/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace OCLRT {

template <>
int HwInfoConfigHw<IGFX_CANNONLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

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

    pSkuTable->ftrGpGpuMidBatchPreempt = true;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = true;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = false;
    pSkuTable->ftr3dMidBatchPreempt = true;
    pSkuTable->ftr3dObjectLevelPreempt = true;
    pSkuTable->ftr3dMidBatchPreempt = true;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = true;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = true;

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

    pSkuTable->ftrAstcHdr2D = true;
    pSkuTable->ftrAstcLdr2D = true;
    pSkuTable->ftrTileY = true;

    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    pWaTable->waSendMIFLUSHBeforeVFE = true;
    pWaTable->waReportPerfCountUseGlobalContextID = true;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    if ((1 << hwInfo->pPlatform->usRevId) & 0x3) {
        pWaTable->waFbcLinearSurfaceStride = true;
    }
    if ((1 << hwInfo->pPlatform->usRevId) & 0x1) {
        pWaTable->waEncryptedEdramOnlyPartials = true;
    }
    return 0;
}

template class HwInfoConfigHw<IGFX_CANNONLAKE>;
} // namespace OCLRT
