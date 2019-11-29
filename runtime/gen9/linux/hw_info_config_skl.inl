/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_cmds.h"
#include "core/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_SKYLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    if (gtSystemInfo->SubSliceCount > 3) {
        gtSystemInfo->SliceCount = 2;
    } else {
        gtSystemInfo->SliceCount = 1;
    }

    gtSystemInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    gtSystemInfo->VDBoxInfo.Instances.Bits.VDBox0Enabled = 1;
    gtSystemInfo->VEBoxInfo.IsValid = true;
    gtSystemInfo->VDBoxInfo.IsValid = true;

    if (hwInfo->platform.usDeviceID == ISKL_GT3e_ULT_DEVICE_F0_ID_540 ||
        hwInfo->platform.usDeviceID == ISKL_GT3e_ULT_DEVICE_F0_ID_550 ||
        hwInfo->platform.usDeviceID == ISKL_GT3_MEDIA_SERV_DEVICE_F0_ID) {
        gtSystemInfo->EdramSizeInKb = 64 * 1024;
    }

    if (hwInfo->platform.usDeviceID == ISKL_GT4_HALO_MOBL_DEVICE_F0_ID ||
        hwInfo->platform.usDeviceID == ISKL_GT4_WRK_DEVICE_F0_ID) {
        gtSystemInfo->EdramSizeInKb = 128 * 1024;
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
} // namespace NEO
