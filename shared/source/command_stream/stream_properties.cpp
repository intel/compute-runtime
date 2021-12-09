/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

#include "shared/source/kernel/grf_config.h"

using namespace NEO;

void StateComputeModeProperties::setProperties(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy) {
    clearIsDirty();

    int32_t isCoherencyRequired = (requiresCoherency ? 1 : 0);
    this->isCoherencyRequired.set(isCoherencyRequired);

    int32_t largeGrfMode = (numGrfRequired == GrfConfig::LargeGrfNumber ? 1 : 0);
    this->largeGrfMode.set(largeGrfMode);

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

    if (DebugManager.flags.OverrideThreadArbitrationPolicy.get() != -1) {
        threadArbitrationPolicy = static_cast<uint32_t>(DebugManager.flags.OverrideThreadArbitrationPolicy.get());
    }
    this->threadArbitrationPolicy.set(threadArbitrationPolicy);
}

void StateComputeModeProperties::setProperties(const StateComputeModeProperties &properties) {
    clearIsDirty();

    isCoherencyRequired.set(properties.isCoherencyRequired.value);
    largeGrfMode.set(properties.largeGrfMode.value);
    zPassAsyncComputeThreadLimit.set(properties.zPassAsyncComputeThreadLimit.value);
    pixelAsyncComputeThreadLimit.set(properties.pixelAsyncComputeThreadLimit.value);
    threadArbitrationPolicy.set(properties.threadArbitrationPolicy.value);
}

bool StateComputeModeProperties::isDirty() const {
    return isCoherencyRequired.isDirty || largeGrfMode.isDirty || zPassAsyncComputeThreadLimit.isDirty ||
           pixelAsyncComputeThreadLimit.isDirty || threadArbitrationPolicy.isDirty;
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
    largeGrfMode.isDirty = false;
    zPassAsyncComputeThreadLimit.isDirty = false;
    pixelAsyncComputeThreadLimit.isDirty = false;
    threadArbitrationPolicy.isDirty = false;
}

void FrontEndProperties::setProperties(bool isCooperativeKernel, bool disableOverdispatch, int32_t engineInstancedDevice,
                                       const HardwareInfo &hwInfo) {
    clearIsDirty();

    this->computeDispatchAllWalkerEnable.set(isCooperativeKernel);
    this->disableOverdispatch.set(disableOverdispatch);
    this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
}

void FrontEndProperties::setProperties(const FrontEndProperties &properties) {
    clearIsDirty();

    disableOverdispatch.set(properties.disableOverdispatch.value);
    singleSliceDispatchCcsMode.set(properties.singleSliceDispatchCcsMode.value);
    computeDispatchAllWalkerEnable.set(properties.computeDispatchAllWalkerEnable.value);
}

bool FrontEndProperties::isDirty() const {
    return disableOverdispatch.isDirty || singleSliceDispatchCcsMode.isDirty || computeDispatchAllWalkerEnable.isDirty;
}

void FrontEndProperties::clearIsDirty() {
    disableOverdispatch.isDirty = false;
    singleSliceDispatchCcsMode.isDirty = false;
    computeDispatchAllWalkerEnable.isDirty = false;
}
