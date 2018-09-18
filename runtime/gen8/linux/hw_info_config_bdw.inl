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
int HwInfoConfigHw<IGFX_BROADWELL>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

    // There is no interface to read total slice count from drm/i915, so we
    // derive this from the number of EUs and subslices.
    // otherwise there is one slice.
    if (pSysInfo->SubSliceCount > 3) {
        pSysInfo->SliceCount = 2;
    } else {
        pSysInfo->SliceCount = 1;
    }

    pSkuTable->ftrPPGTT = 1;
    pSkuTable->ftrSVM = 1;
    pSkuTable->ftrL3IACoherency = 1;
    pSkuTable->ftrIA32eGfxPTEs = 1;

    pSkuTable->ftrFbc = 1;
    pSkuTable->ftrFbc2AddressTranslation = 1;
    pSkuTable->ftrFbcBlitterTracking = 1;
    pSkuTable->ftrFbcCpuTracking = 1;

    pWaTable->waDisableLSQCROPERFforOCL = 1;
    pWaTable->waReportPerfCountUseGlobalContextID = 1;
    pWaTable->waUseVAlign16OnTileXYBpp816 = 1;
    pWaTable->waModifyVFEStateAfterGPGPUPreemption = 1;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = 1;

    if (hwInfo->pPlatform->usDeviceID == IBDW_GT3_HALO_MOBL_DEVICE_F0_ID ||
        hwInfo->pPlatform->usDeviceID == IBDW_GT3_SERV_DEVICE_F0_ID) {
        pSysInfo->EdramSizeInKb = 128 * 1024;
    }
    return 0;
}

template class HwInfoConfigHw<IGFX_BROADWELL>;

} // namespace OCLRT
