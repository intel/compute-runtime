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
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    // There is no interface to read total slice count from drm/i915, so we
    // derive this from the number of EUs and subslices.
    // otherwise there is one slice.
    if (gtSystemInfo->SubSliceCount > 3) {
        gtSystemInfo->SliceCount = 2;
    } else {
        gtSystemInfo->SliceCount = 1;
    }

    if (hwInfo->platform.usDeviceID == IBDW_GT3_HALO_MOBL_DEVICE_F0_ID ||
        hwInfo->platform.usDeviceID == IBDW_GT3_SERV_DEVICE_F0_ID) {
        gtSystemInfo->EdramSizeInKb = 128 * 1024;
    }
    return 0;
}

template class HwInfoConfigHw<IGFX_BROADWELL>;

} // namespace NEO
