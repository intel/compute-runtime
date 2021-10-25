/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "hw_cmds.h"

namespace NEO {

int HwInfoConfig::configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface) {
    auto &hwHelper = HwHelper::get(outHwInfo->platform.eRenderCoreFamily);
    auto &hwInfoConfig = *HwInfoConfig::get(outHwInfo->platform.eProductFamily);
    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(outHwInfo->platform.eProductFamily);
    outHwInfo->capabilityTable.ftrSvm = outHwInfo->featureTable.ftrSVM;

    hwHelper.adjustDefaultEngineType(outHwInfo);
    outHwInfo->capabilityTable.defaultEngineType = getChosenEngineType(*outHwInfo);

    hwInfoConfig.setCapabilityCoherencyFlag(*outHwInfo, outHwInfo->capabilityTable.ftrSupportsCoherency);
    outHwInfo->capabilityTable.ftrSupportsCoherency &= inHwInfo->featureTable.ftrL3IACoherency;

    PreemptionHelper::adjustDefaultPreemptionMode(outHwInfo->capabilityTable,
                                                  compilerHwInfoConfig->isMidThreadPreemptionSupported(*outHwInfo),
                                                  static_cast<bool>(outHwInfo->featureTable.ftrGpGpuThreadGroupLevelPreempt),
                                                  static_cast<bool>(outHwInfo->featureTable.ftrGpGpuMidBatchPreempt));

    if (DebugManager.flags.OverridePreemptionSurfaceSizeInMb.get() >= 0) {
        outHwInfo->gtSystemInfo.CsrSizeInMb = static_cast<uint32_t>(DebugManager.flags.OverridePreemptionSurfaceSizeInMb.get());
    }
    outHwInfo->capabilityTable.requiredPreemptionSurfaceSize = outHwInfo->gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    hwHelper.adjustPreemptionSurfaceSize(outHwInfo->capabilityTable.requiredPreemptionSurfaceSize);

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
        *outHwInfo = {};
    }
    return ret;
}

} // namespace NEO
