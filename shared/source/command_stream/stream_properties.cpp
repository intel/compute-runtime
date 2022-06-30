/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/hw_info_config.h"

using namespace NEO;

void StateComputeModeProperties::setProperties(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, PreemptionMode devicePreemptionMode,
                                               const HardwareInfo &hwInfo) {
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    clearIsDirty();

    int32_t isCoherencyRequired = (requiresCoherency ? 1 : 0);
    this->isCoherencyRequired.set(isCoherencyRequired);

    bool reportNumGrf = hwInfoConfig.isGrfNumReportedWithScm();
    if (reportNumGrf && (this->largeGrfMode.value == -1 || numGrfRequired != GrfConfig::NotApplicable)) {
        int32_t largeGrfMode = (numGrfRequired == GrfConfig::LargeGrfNumber ? 1 : 0);
        this->largeGrfMode.set(largeGrfMode);
    }

    int32_t zPassAsyncComputeThreadLimit = -1;
    if (DebugManager.flags.ForceZPassAsyncComputeThreadLimit.get() != -1) {
        zPassAsyncComputeThreadLimit = DebugManager.flags.ForceZPassAsyncComputeThreadLimit.get();
    }
    this->zPassAsyncComputeThreadLimit.set(zPassAsyncComputeThreadLimit);

    int32_t pixelAsyncComputeThreadLimit = -1;
    if (DebugManager.flags.ForcePixelAsyncComputeThreadLimit.get() != -1) {
        pixelAsyncComputeThreadLimit = DebugManager.flags.ForcePixelAsyncComputeThreadLimit.get();
    }
    this->pixelAsyncComputeThreadLimit.set(pixelAsyncComputeThreadLimit);

    bool setDefaultThreadArbitrationPolicy = (threadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent) &&
                                             (NEO::DebugManager.flags.ForceDefaultThreadArbitrationPolicyIfNotSpecified.get() ||
                                              (this->threadArbitrationPolicy.value == ThreadArbitrationPolicy::NotPresent));
    if (setDefaultThreadArbitrationPolicy) {
        threadArbitrationPolicy = hwHelper.getDefaultThreadArbitrationPolicy();
    }
    if (DebugManager.flags.OverrideThreadArbitrationPolicy.get() != -1) {
        threadArbitrationPolicy = DebugManager.flags.OverrideThreadArbitrationPolicy.get();
    }
    bool reportThreadArbitrationPolicy = hwInfoConfig.isThreadArbitrationPolicyReportedWithScm();
    if (reportThreadArbitrationPolicy) {
        this->threadArbitrationPolicy.set(threadArbitrationPolicy);
    }

    if (hwHelper.isDevicePreemptionModeTrackedInScm()) {
        this->devicePreemptionMode.set(static_cast<int32_t>(devicePreemptionMode));
    }

    setPropertiesExtra(reportNumGrf, reportThreadArbitrationPolicy);
}

void StateComputeModeProperties::setProperties(const StateComputeModeProperties &properties) {
    clearIsDirty();

    isCoherencyRequired.set(properties.isCoherencyRequired.value);
    largeGrfMode.set(properties.largeGrfMode.value);
    zPassAsyncComputeThreadLimit.set(properties.zPassAsyncComputeThreadLimit.value);
    pixelAsyncComputeThreadLimit.set(properties.pixelAsyncComputeThreadLimit.value);
    threadArbitrationPolicy.set(properties.threadArbitrationPolicy.value);
    devicePreemptionMode.set(properties.devicePreemptionMode.value);

    setPropertiesExtra(properties);
}

bool StateComputeModeProperties::isDirty() const {
    return isCoherencyRequired.isDirty || largeGrfMode.isDirty || zPassAsyncComputeThreadLimit.isDirty ||
           pixelAsyncComputeThreadLimit.isDirty || threadArbitrationPolicy.isDirty || devicePreemptionMode.isDirty || isDirtyExtra();
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
    largeGrfMode.isDirty = false;
    zPassAsyncComputeThreadLimit.isDirty = false;
    pixelAsyncComputeThreadLimit.isDirty = false;
    threadArbitrationPolicy.isDirty = false;
    devicePreemptionMode.isDirty = false;

    clearIsDirtyExtra();
}

void FrontEndProperties::setProperties(bool isCooperativeKernel, bool disableEUFusion, bool disableOverdispatch,
                                       int32_t engineInstancedDevice, const HardwareInfo &hwInfo) {
    clearIsDirty();

    this->computeDispatchAllWalkerEnable.set(isCooperativeKernel);
    this->disableEUFusion.set(disableEUFusion);
    this->disableOverdispatch.set(disableOverdispatch);
    this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
}

void FrontEndProperties::setProperties(const FrontEndProperties &properties) {
    clearIsDirty();

    disableOverdispatch.set(properties.disableOverdispatch.value);
    disableEUFusion.set(properties.disableEUFusion.value);
    singleSliceDispatchCcsMode.set(properties.singleSliceDispatchCcsMode.value);
    computeDispatchAllWalkerEnable.set(properties.computeDispatchAllWalkerEnable.value);
}

bool FrontEndProperties::isDirty() const {
    return disableOverdispatch.isDirty || disableEUFusion.isDirty || singleSliceDispatchCcsMode.isDirty ||
           computeDispatchAllWalkerEnable.isDirty;
}

void FrontEndProperties::clearIsDirty() {
    disableEUFusion.isDirty = false;
    disableOverdispatch.isDirty = false;
    singleSliceDispatchCcsMode.isDirty = false;
    computeDispatchAllWalkerEnable.isDirty = false;
}
