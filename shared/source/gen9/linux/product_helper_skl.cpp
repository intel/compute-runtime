/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_skl.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_bdw_and_later.inl"

constexpr static auto gfxProduct = IGFX_SKYLAKE;

#include "shared/source/gen9/skl/os_agnostic_product_helper_skl.inl"
#include "shared/source/os_interface/product_helper_before_gen12lp.inl"

namespace NEO {

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (nullptr == osIface) {
        return 0;
    }
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    gtSystemInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    gtSystemInfo->VDBoxInfo.Instances.Bits.VDBox0Enabled = 1;
    gtSystemInfo->VEBoxInfo.IsValid = true;
    gtSystemInfo->VDBoxInfo.IsValid = true;

    if (hwInfo->platform.usDeviceID == 0x1926 ||
        hwInfo->platform.usDeviceID == 0x1927 ||
        hwInfo->platform.usDeviceID == 0x192D) {
        gtSystemInfo->EdramSizeInKb = 64 * 1024;
    }

    if (hwInfo->platform.usDeviceID == 0x193B ||
        hwInfo->platform.usDeviceID == 0x193D) {
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

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
