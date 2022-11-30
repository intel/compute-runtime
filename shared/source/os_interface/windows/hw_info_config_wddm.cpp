/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "hw_cmds.h"

namespace NEO {

int HwInfoConfig::configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironemnt) {
    auto &coreHelper = rootDeviceEnvironemnt.getHelper<CoreHelper>();
    auto &productHelper = rootDeviceEnvironemnt.getHelper<ProductHelper>();
    auto compilerProductHelper = CompilerProductHelper::get(outHwInfo->platform.eProductFamily);
    outHwInfo->capabilityTable.ftrSvm = outHwInfo->featureTable.flags.ftrSVM;

    coreHelper.adjustDefaultEngineType(outHwInfo);
    outHwInfo->capabilityTable.defaultEngineType = getChosenEngineType(*outHwInfo);

    productHelper.setCapabilityCoherencyFlag(*outHwInfo, outHwInfo->capabilityTable.ftrSupportsCoherency);
    outHwInfo->capabilityTable.ftrSupportsCoherency &= inHwInfo->featureTable.flags.ftrL3IACoherency;

    PreemptionHelper::adjustDefaultPreemptionMode(outHwInfo->capabilityTable,
                                                  compilerProductHelper->isMidThreadPreemptionSupported(*outHwInfo),
                                                  static_cast<bool>(outHwInfo->featureTable.flags.ftrGpGpuThreadGroupLevelPreempt),
                                                  static_cast<bool>(outHwInfo->featureTable.flags.ftrGpGpuMidBatchPreempt));

    if (DebugManager.flags.OverridePreemptionSurfaceSizeInMb.get() >= 0) {
        outHwInfo->gtSystemInfo.CsrSizeInMb = static_cast<uint32_t>(DebugManager.flags.OverridePreemptionSurfaceSizeInMb.get());
    }
    outHwInfo->capabilityTable.requiredPreemptionSurfaceSize = outHwInfo->gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    coreHelper.adjustPreemptionSurfaceSize(outHwInfo->capabilityTable.requiredPreemptionSurfaceSize);

    auto &kmdNotifyProperties = outHwInfo->capabilityTable.kmdNotifyProperties;
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableKmdNotify.get(), kmdNotifyProperties.enableKmdNotify);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.get(), kmdNotifyProperties.delayKmdNotifyMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleep.get(), kmdNotifyProperties.enableQuickKmdSleep);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.get(), kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleepForDirectSubmission.get(), kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideDelayQuickKmdSleepForDirectSubmissionMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);

    auto osInterface = rootDeviceEnvironemnt.osInterface.get();
    // Product specific config
    int ret = configureHardwareCustom(outHwInfo, osInterface);
    if (ret != 0) {
        *outHwInfo = {};
    }

    if (DebugManager.flags.ForceImagesSupport.get() != -1) {
        outHwInfo->capabilityTable.supportsImages = DebugManager.flags.ForceImagesSupport.get();
    }

    return ret;
}

} // namespace NEO
