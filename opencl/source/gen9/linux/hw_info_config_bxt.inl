/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_BROXTON>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }

    Drm *drm = osIface->get()->getDrm();
    FeatureTable *featureTable = &hwInfo->featureTable;
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    gtSystemInfo->SliceCount = 1;

    gtSystemInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled = 1;
    gtSystemInfo->VEBoxInfo.IsValid = true;

    int enabled = 0;
    int retVal = drm->getEnabledPooledEu(enabled);
    if (retVal == 0) {
        featureTable->ftrPooledEuEnabled = (enabled != 0);
    }
    if (enabled) {
        int num = 0;
        retVal = drm->getMinEuInPool(num);
        if (retVal == 0 && ((num == 3) || (num == 6) || (num == 9))) {
            gtSystemInfo->EuCountPerPoolMin = static_cast<uint32_t>(num);
        }
        //in case of failure or not getting right values, fallback to default
        else {
            if (gtSystemInfo->SubSliceCount == 3) {
                // Native 3x6, PooledEU 2x9
                gtSystemInfo->EuCountPerPoolMin = 9;
            } else {
                // Native 3x6 fused down to 2x6, PooledEU worst case 3+9
                gtSystemInfo->EuCountPerPoolMin = 3;
            }
        }
        gtSystemInfo->EuCountPerPoolMax = gtSystemInfo->EUCount - gtSystemInfo->EuCountPerPoolMin;
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

template class HwInfoConfigHw<IGFX_BROXTON>;
} // namespace NEO
