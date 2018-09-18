/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace OCLRT {

template <>
int HwInfoConfigHw<IGFX_COFFEELAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

    if (pSysInfo->SubSliceCount > 3) {
        pSysInfo->SliceCount = 2;
    } else {
        pSysInfo->SliceCount = 1;
    }

    pSysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    pSysInfo->VEBoxInfo.IsValid = true;
    pSkuTable->ftrVEBOX = 1;

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

    pWaTable->waEnablePreemptionGranularityControlByUMD = 1;
    pWaTable->waSendMIFLUSHBeforeVFE = 1;
    pWaTable->waReportPerfCountUseGlobalContextID = 1;
    pWaTable->waMsaa8xTileYDepthPitchAlignment = 1;
    pWaTable->waLosslessCompressionSurfaceStride = 1;
    pWaTable->waFbcLinearSurfaceStride = 1;
    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = 1;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = 1;

    if (hwInfo->pPlatform->usDeviceID == ICFL_GT3_ULT_28W_DEVICE_F0_ID ||
        hwInfo->pPlatform->usDeviceID == ICFL_GT3_ULT_15W_DEVICE_F0_ID) {
        pSysInfo->EdramSizeInKb = 64 * 1024;
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

template class HwInfoConfigHw<IGFX_COFFEELAKE>;
} // namespace OCLRT
