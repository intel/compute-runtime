/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
