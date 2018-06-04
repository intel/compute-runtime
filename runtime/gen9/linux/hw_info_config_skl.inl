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
#include "runtime/gen_common/hw_cmds.h"

namespace OCLRT {

template <>
int HwInfoConfigHw<IGFX_SKYLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

    if (pSysInfo->SubSliceCount > 3) {
        pSysInfo->SliceCount = 2;
    } else {
        pSysInfo->SliceCount = 1;
    }

    pSysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    pSysInfo->VDBoxInfo.Instances.Bits.VDBox0Enabled = 1;
    pSysInfo->VEBoxInfo.IsValid = true;
    pSysInfo->VDBoxInfo.IsValid = true;

    pSkuTable->ftrGpGpuMidBatchPreempt = 1;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = 1;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = 0;
    pSkuTable->ftr3dMidBatchPreempt = 1;
    pSkuTable->ftr3dObjectLevelPreempt = 1;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = 1;

    pSkuTable->ftrPPGTT = 1;
    pSkuTable->ftrSVM = 1;
    pSkuTable->ftrL3IACoherency = 1;
    pSkuTable->ftrIA32eGfxPTEs = 1;

    pSkuTable->ftrDisplayYTiling = 1;
    pSkuTable->ftrTranslationTable = 1;
    pSkuTable->ftrUserModeTranslationTable = 1;

    pSkuTable->ftrEnableGuC = 1;

    pSkuTable->ftrFbc = 1;
    pSkuTable->ftrFbc2AddressTranslation = 1;
    pSkuTable->ftrFbcBlitterTracking = 1;
    pSkuTable->ftrFbcCpuTracking = 1;

    pSkuTable->ftrVcs2 = pSkuTable->ftrGT3 || pSkuTable->ftrGT4;
    pSkuTable->ftrVEBOX = 1;
    pSkuTable->ftrSingleVeboxSlice = pSkuTable->ftrGT1 || pSkuTable->ftrGT2;

    pWaTable->waEnablePreemptionGranularityControlByUMD = 1;
    pWaTable->waSendMIFLUSHBeforeVFE = 1;
    pWaTable->waReportPerfCountUseGlobalContextID = 1;
    pWaTable->waDisableLSQCROPERFforOCL = 1;
    pWaTable->waMsaa8xTileYDepthPitchAlignment = 1;
    pWaTable->waLosslessCompressionSurfaceStride = 1;
    pWaTable->waFbcLinearSurfaceStride = 1;
    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = 1;
    pWaTable->waEncryptedEdramOnlyPartials = 1;
    pWaTable->waDisableEdramForDisplayRT = 1;
    pWaTable->waForcePcBbFullCfgRestore = 1;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = 1;

    if ((1 << hwInfo->pPlatform->usRevId) & 0x0eu) {
        pWaTable->waCompressedResourceRequiresConstVA21 = 1;
    }
    if ((1 << hwInfo->pPlatform->usRevId) & 0x0fu) {
        pWaTable->waDisablePerCtxtPreemptionGranularityControl = 1;
        pWaTable->waModifyVFEStateAfterGPGPUPreemption = 1;
    }
    if ((1 << hwInfo->pPlatform->usRevId) & 0x3f) {
        pWaTable->waCSRUncachable = 1;
    }

    if (hwInfo->pPlatform->usDeviceID == ISKL_GT3e_ULT_DEVICE_F0_ID_540 ||
        hwInfo->pPlatform->usDeviceID == ISKL_GT3e_ULT_DEVICE_F0_ID_550 ||
        hwInfo->pPlatform->usDeviceID == ISKL_GT3_MEDIA_SERV_DEVICE_F0_ID) {
        pSysInfo->EdramSizeInKb = 64 * 1024;
    }

    if (hwInfo->pPlatform->usDeviceID == ISKL_GT4_HALO_MOBL_DEVICE_F0_ID ||
        hwInfo->pPlatform->usDeviceID == ISKL_GT4_WRK_DEVICE_F0_ID) {
        pSysInfo->EdramSizeInKb = 128 * 1024;
    }

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.enableQuickKmdSleep = true;
    kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 50000;
    kmdNotifyProperties.delayQuickKmdSleepMicroseconds = 5000;
    kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds = 200000;
    return 0;
}

template class HwInfoConfigHw<IGFX_SKYLAKE>;
} // namespace OCLRT
