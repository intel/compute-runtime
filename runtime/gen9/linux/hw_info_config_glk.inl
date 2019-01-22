/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_interface.h"

namespace OCLRT {

template <>
int HwInfoConfigHw<IGFX_GEMINILAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    Drm *drm = osIface->get()->getDrm();
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

    pSysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    pSysInfo->VEBoxInfo.IsValid = true;

    pSkuTable->ftrGpGpuMidBatchPreempt = true;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = true;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = false;
    pSkuTable->ftr3dMidBatchPreempt = true;
    pSkuTable->ftr3dObjectLevelPreempt = true;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = true;

    pSkuTable->ftrLCIA = true;
    pSkuTable->ftrPPGTT = true;
    pSkuTable->ftrL3IACoherency = true;
    pSkuTable->ftrIA32eGfxPTEs = true;

    pSkuTable->ftrTranslationTable = true;
    pSkuTable->ftrUserModeTranslationTable = true;
    pSkuTable->ftrEnableGuC = true;
    pSkuTable->ftrTileMappedResource = true;
    pSkuTable->ftrULT = true;
    pSkuTable->ftrAstcHdr2D = true;
    pSkuTable->ftrAstcLdr2D = true;
    pSkuTable->ftrTileY = true;

    pWaTable->waLLCCachingUnsupported = true;
    pWaTable->waMsaa8xTileYDepthPitchAlignment = true;
    pWaTable->waFbcLinearSurfaceStride = true;
    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    pWaTable->waEnablePreemptionGranularityControlByUMD = true;
    pWaTable->waSendMIFLUSHBeforeVFE = true;
    pWaTable->waForcePcBbFullCfgRestore = true;
    pWaTable->waReportPerfCountUseGlobalContextID = true;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    int enabled = 0;
    int retVal = drm->getEnabledPooledEu(enabled);
    if (retVal == 0) {
        pSkuTable->ftrPooledEuEnabled = (enabled != 0);
    }
    if (enabled) {
        int num = 0;
        retVal = drm->getMinEuInPool(num);
        if (retVal == 0 && ((num == 3) || (num == 6) || (num == 9))) {
            pSysInfo->EuCountPerPoolMin = static_cast<uint32_t>(num);
        }
        //in case of failure or not getting right values, fallback to default
        else {
            if (pSysInfo->SubSliceCount == 3) {
                // Native 3x6, PooledEU 2x9
                pSysInfo->EuCountPerPoolMin = 9;
            } else {
                // Native 3x6 fused down to 2x6, PooledEU worst case 3+9
                pSysInfo->EuCountPerPoolMin = 3;
            }
        }
        pSysInfo->EuCountPerPoolMax = pSysInfo->EUCount - pSysInfo->EuCountPerPoolMin;
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

template class HwInfoConfigHw<IGFX_GEMINILAKE>;
} // namespace OCLRT
