/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_cmds.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_constants.h"

#include "instrumentation.h"

namespace NEO {

HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT] = {};

int HwInfoConfig::configureHwInfo(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface) {
    HwHelper &hwHelper = HwHelper::get(outHwInfo->platform.eRenderCoreFamily);

    outHwInfo->capabilityTable.ftrSvm = outHwInfo->featureTable.ftrSVM;

    hwHelper.adjustDefaultEngineType(outHwInfo);
    outHwInfo->capabilityTable.defaultEngineType = getChosenEngineType(*outHwInfo);

    hwHelper.setCapabilityCoherencyFlag(outHwInfo, outHwInfo->capabilityTable.ftrSupportsCoherency);
    outHwInfo->capabilityTable.ftrSupportsCoherency &= inHwInfo->featureTable.ftrL3IACoherency;

    PreemptionHelper::adjustDefaultPreemptionMode(outHwInfo->capabilityTable,
                                                  static_cast<bool>(outHwInfo->featureTable.ftrGpGpuMidThreadLevelPreempt),
                                                  static_cast<bool>(outHwInfo->featureTable.ftrGpGpuThreadGroupLevelPreempt),
                                                  static_cast<bool>(outHwInfo->featureTable.ftrGpGpuMidBatchPreempt));
    outHwInfo->capabilityTable.requiredPreemptionSurfaceSize = outHwInfo->gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;

    outHwInfo->capabilityTable.instrumentationEnabled =
        (outHwInfo->capabilityTable.instrumentationEnabled && haveInstrumentation);

    auto &kmdNotifyProperties = outHwInfo->capabilityTable.kmdNotifyProperties;
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableKmdNotify.get(), kmdNotifyProperties.enableKmdNotify);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.get(), kmdNotifyProperties.delayKmdNotifyMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleep.get(), kmdNotifyProperties.enableQuickKmdSleep);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.get(), kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);

    // Product specific config
    int ret = configureHardwareCustom(outHwInfo, osIface);
    if (ret != 0) {
        outHwInfo = {};
    }
    return ret;
}

} // namespace NEO
