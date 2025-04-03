/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/kmd_notify_properties.h"

namespace NEO {

ProductHelperCreateFunctionType productHelperFactory[IGFX_MAX_PRODUCT] = {};

void ProductHelper::setupPreemptionSurfaceSize(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.OverridePreemptionSurfaceSizeInMb.get() >= 0) {
        hwInfo.gtSystemInfo.CsrSizeInMb = static_cast<uint32_t>(debugManager.flags.OverridePreemptionSurfaceSizeInMb.get());
    }

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    hwInfo.capabilityTable.requiredPreemptionSurfaceSize = hwInfo.gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    gfxCoreHelper.adjustPreemptionSurfaceSize(hwInfo.capabilityTable.requiredPreemptionSurfaceSize, rootDeviceEnvironment);
}

void ProductHelper::setupKmdNotifyProperties(KmdNotifyProperties &kmdNotifyProperties) {
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableKmdNotify.get(), kmdNotifyProperties.enableKmdNotify);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideKmdNotifyDelayMicroseconds.get(), kmdNotifyProperties.delayKmdNotifyMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableQuickKmdSleep.get(), kmdNotifyProperties.enableQuickKmdSleep);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.get(), kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideEnableQuickKmdSleepForDirectSubmission.get(), kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    KmdNotifyHelper::overrideFromDebugVariable(debugManager.flags.OverrideDelayQuickKmdSleepForDirectSubmissionMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

int ProductHelper::setupProductSpecificConfig(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto osInterface = rootDeviceEnvironment.osInterface.get();
    setRenderCompressedFlags(hwInfo);
    int ret = configureHardwareCustom(&hwInfo, osInterface);
    if (ret != 0) {
        hwInfo = {};
    }
    return ret;
}
void ProductHelper::setupPreemptionMode(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment, bool kmdPreemptionSupport) {
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    PreemptionHelper::adjustDefaultPreemptionMode(hwInfo.capabilityTable,
                                                  compilerProductHelper.isMidThreadPreemptionSupported(hwInfo) && kmdPreemptionSupport,
                                                  static_cast<bool>(hwInfo.featureTable.flags.ftrGpGpuThreadGroupLevelPreempt) && kmdPreemptionSupport,
                                                  static_cast<bool>(hwInfo.featureTable.flags.ftrGpGpuMidBatchPreempt) && kmdPreemptionSupport);
}

void ProductHelper::setupImageSupport(HardwareInfo &hwInfo) {
    if (debugManager.flags.ForceImagesSupport.get() != -1) {
        hwInfo.capabilityTable.supportsImages = debugManager.flags.ForceImagesSupport.get();
    }
}

void ProductHelper::setupDefaultEngineType(HardwareInfo &hwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    gfxCoreHelper.adjustDefaultEngineType(&hwInfo, *this, rootDeviceEnvironment.ailConfiguration.get());
    hwInfo.capabilityTable.defaultEngineType = getChosenEngineType(hwInfo);
}
} // namespace NEO
