/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/hw_info_config.h"

#include "core/command_stream/preemption.h"
#include "core/helpers/hw_cmds.h"
#include "core/helpers/hw_helper.h"
#include "core/helpers/hw_info.h"
#include "core/memory_manager/memory_constants.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include "instrumentation.h"

namespace NEO {

HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT] = {};

int HwInfoConfig::configureHwInfo(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface) {
    HwHelper &hwHelper = HwHelper::get(outHwInfo->platform.eRenderCoreFamily);

    outHwInfo->capabilityTable.ftrSvm = outHwInfo->featureTable.ftrSVM;

    hwHelper.adjustDefaultEngineType(outHwInfo);
    outHwInfo->capabilityTable.defaultEngineType = getChosenEngineType(*outHwInfo);

    hwHelper.setCapabilityCoherencyFlag(outHwInfo, outHwInfo->capabilityTable.ftrSupportsCoherency);

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
