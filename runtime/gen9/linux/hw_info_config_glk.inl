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

    pSkuTable->ftrGpGpuMidBatchPreempt = 1;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = 1;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = 0;
    pSkuTable->ftr3dMidBatchPreempt = 1;
    pSkuTable->ftr3dObjectLevelPreempt = 1;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = 1;

    pSkuTable->ftrLCIA = 1;
    pSkuTable->ftrPPGTT = 1;
    pSkuTable->ftrL3IACoherency = 1;
    pSkuTable->ftrIA32eGfxPTEs = 1;

    pSkuTable->ftrTranslationTable = 1;
    pSkuTable->ftrUserModeTranslationTable = 1;
    pSkuTable->ftrEnableGuC = 1;
    pSkuTable->ftrTileMappedResource = 1;
    pSkuTable->ftrULT = 1;
    pSkuTable->ftrAstcHdr2D = 1;
    pSkuTable->ftrAstcLdr2D = 1;

    pWaTable->waLLCCachingUnsupported = 1;
    pWaTable->waMsaa8xTileYDepthPitchAlignment = 1;
    pWaTable->waFbcLinearSurfaceStride = 1;
    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = 1;
    pWaTable->waEnablePreemptionGranularityControlByUMD = 1;
    pWaTable->waSendMIFLUSHBeforeVFE = 1;
    pWaTable->waForcePcBbFullCfgRestore = 1;
    pWaTable->waReportPerfCountUseGlobalContextID = 1;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = 1;

    int enabled = 0;
    int retVal = drm->getEnabledPooledEu(enabled);
    if (retVal == 0) {
        pSkuTable->ftrPooledEuEnabled = (enabled != 0) ? 1 : 0;
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
