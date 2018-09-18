/*
 * Copyright (C) 2017-2018 Intel Corporation
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

    pSkuTable->ftrGpGpuMidBatchPreempt = 1;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = 1;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = 0;
    pSkuTable->ftr3dMidBatchPreempt = 1;
    pSkuTable->ftr3dObjectLevelPreempt = 1;
    pSkuTable->ftr3dMidBatchPreempt = 1;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = 1;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = 1;

    pSkuTable->ftrPPGTT = 1;
    pSkuTable->ftrSVM = 1;
    pSkuTable->ftrL3IACoherency = 1;
    pSkuTable->ftrIA32eGfxPTEs = 1;
    pSkuTable->ftrStandardMipTailFormat = 1;

    pSkuTable->ftrDisplayYTiling = 1;
    pSkuTable->ftrTranslationTable = 1;
    pSkuTable->ftrUserModeTranslationTable = 1;
    pSkuTable->ftrTileMappedResource = 1;
    pSkuTable->ftrEnableGuC = 1;

    pSkuTable->ftrFbc = 1;
    pSkuTable->ftrFbc2AddressTranslation = 1;
    pSkuTable->ftrFbcBlitterTracking = 1;
    pSkuTable->ftrFbcCpuTracking = 1;

    pSkuTable->ftrAstcHdr2D = 1;
    pSkuTable->ftrAstcLdr2D = 1;

    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = 1;
    pWaTable->waSendMIFLUSHBeforeVFE = 1;
    pWaTable->waReportPerfCountUseGlobalContextID = 1;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = 1;

    if ((1 << hwInfo->pPlatform->usRevId) & 0x3) {
        pWaTable->waFbcLinearSurfaceStride = 1;
    }
    if ((1 << hwInfo->pPlatform->usRevId) & 0x1) {
        pWaTable->waEncryptedEdramOnlyPartials = 1;
    }
    return 0;
}

template class HwInfoConfigHw<IGFX_CANNONLAKE>;
} // namespace OCLRT
