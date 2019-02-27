/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

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

    pSkuTable->ftrGpGpuMidBatchPreempt = true;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = true;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = false;
    pSkuTable->ftr3dMidBatchPreempt = true;
    pSkuTable->ftr3dObjectLevelPreempt = true;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = true;

    pSkuTable->ftrPPGTT = true;
    pSkuTable->ftrSVM = true;
    pSkuTable->ftrL3IACoherency = true;
    pSkuTable->ftrIA32eGfxPTEs = true;

    pSkuTable->ftrDisplayYTiling = true;
    pSkuTable->ftrTranslationTable = true;
    pSkuTable->ftrUserModeTranslationTable = true;

    pSkuTable->ftrEnableGuC = true;

    pSkuTable->ftrFbc = true;
    pSkuTable->ftrFbc2AddressTranslation = true;
    pSkuTable->ftrFbcBlitterTracking = true;
    pSkuTable->ftrFbcCpuTracking = true;

    pSkuTable->ftrVcs2 = pSkuTable->ftrGT3 || pSkuTable->ftrGT4;
    pSkuTable->ftrVEBOX = true;
    pSkuTable->ftrSingleVeboxSlice = pSkuTable->ftrGT1 || pSkuTable->ftrGT2;
    pSkuTable->ftrTileY = true;

    pWaTable->waEnablePreemptionGranularityControlByUMD = true;
    pWaTable->waSendMIFLUSHBeforeVFE = true;
    pWaTable->waReportPerfCountUseGlobalContextID = true;
    pWaTable->waDisableLSQCROPERFforOCL = true;
    pWaTable->waMsaa8xTileYDepthPitchAlignment = true;
    pWaTable->waLosslessCompressionSurfaceStride = true;
    pWaTable->waFbcLinearSurfaceStride = true;
    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    pWaTable->waEncryptedEdramOnlyPartials = true;
    pWaTable->waDisableEdramForDisplayRT = true;
    pWaTable->waForcePcBbFullCfgRestore = true;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    if ((1 << hwInfo->pPlatform->usRevId) & 0x0eu) {
        pWaTable->waCompressedResourceRequiresConstVA21 = true;
    }
    if ((1 << hwInfo->pPlatform->usRevId) & 0x0fu) {
        pWaTable->waDisablePerCtxtPreemptionGranularityControl = true;
        pWaTable->waModifyVFEStateAfterGPGPUPreemption = true;
    }
    if ((1 << hwInfo->pPlatform->usRevId) & 0x3f) {
        pWaTable->waCSRUncachable = true;
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
