/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_SKYLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);

    pSkuTable->ftrGpGpuMidBatchPreempt = true;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = true;
    pSkuTable->ftrL3IACoherency = true;

    if (pSysInfo->SubSliceCount > 3) {
        pSysInfo->SliceCount = 2;
    } else {
        pSysInfo->SliceCount = 1;
    }

    pSysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    pSysInfo->VDBoxInfo.Instances.Bits.VDBox0Enabled = 1;
    pSysInfo->VEBoxInfo.IsValid = true;
    pSysInfo->VDBoxInfo.IsValid = true;

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
} // namespace NEO
