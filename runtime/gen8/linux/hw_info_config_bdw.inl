/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_BROADWELL>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);

    pSkuTable->ftrL3IACoherency = true;

    // There is no interface to read total slice count from drm/i915, so we
    // derive this from the number of EUs and subslices.
    // otherwise there is one slice.
    if (pSysInfo->SubSliceCount > 3) {
        pSysInfo->SliceCount = 2;
    } else {
        pSysInfo->SliceCount = 1;
    }

    if (hwInfo->pPlatform->usDeviceID == IBDW_GT3_HALO_MOBL_DEVICE_F0_ID ||
        hwInfo->pPlatform->usDeviceID == IBDW_GT3_SERV_DEVICE_F0_ID) {
        pSysInfo->EdramSizeInKb = 128 * 1024;
    }
    return 0;
}

template class HwInfoConfigHw<IGFX_BROADWELL>;

} // namespace NEO
