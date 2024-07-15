/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"

#include "hw_cmds.h"

namespace NEO {

int ProductHelper::configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    outHwInfo->capabilityTable.ftrSvm = outHwInfo->featureTable.flags.ftrSVM;

    gfxCoreHelper.adjustDefaultEngineType(outHwInfo, *this, rootDeviceEnvironment.ailConfiguration.get());
    outHwInfo->capabilityTable.defaultEngineType = getChosenEngineType(*outHwInfo);

    this->setCapabilityCoherencyFlag(*outHwInfo, outHwInfo->capabilityTable.ftrSupportsCoherency);
    outHwInfo->capabilityTable.ftrSupportsCoherency &= inHwInfo->featureTable.flags.ftrL3IACoherency;

    PreemptionHelper::adjustDefaultPreemptionMode(outHwInfo->capabilityTable,
                                                  compilerProductHelper.isMidThreadPreemptionSupported(*outHwInfo),
                                                  static_cast<bool>(outHwInfo->featureTable.flags.ftrGpGpuThreadGroupLevelPreempt),
                                                  static_cast<bool>(outHwInfo->featureTable.flags.ftrGpGpuMidBatchPreempt));

    setupPreemptionSurfaceSize(*outHwInfo, rootDeviceEnvironment);

    auto &kmdNotifyProperties = outHwInfo->capabilityTable.kmdNotifyProperties;
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableKmdNotify.get(), kmdNotifyProperties.enableKmdNotify);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideKmdNotifyDelayMicroseconds.get(), kmdNotifyProperties.delayKmdNotifyMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableQuickKmdSleep.get(), kmdNotifyProperties.enableQuickKmdSleep);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.get(), kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableQuickKmdSleepForDirectSubmission.get(), kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideDelayQuickKmdSleepForDirectSubmissionMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);

    auto osInterface = rootDeviceEnvironment.osInterface.get();
    // Product specific config
    int ret = configureHardwareCustom(outHwInfo, osInterface);
    if (ret != 0) {
        *outHwInfo = {};
    }

    if (debugManager.flags.ForceImagesSupport.get() != -1) {
        outHwInfo->capabilityTable.supportsImages = debugManager.flags.ForceImagesSupport.get();
    }

    return ret;
}

} // namespace NEO
