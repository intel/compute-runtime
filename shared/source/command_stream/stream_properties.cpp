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
}

void StateComputeModeProperties::setProperties(const StateComputeModeProperties &properties) {
    clearIsDirty();

    isCoherencyRequired.set(properties.isCoherencyRequired.value);
    largeGrfMode.set(properties.largeGrfMode.value);
    zPassAsyncComputeThreadLimit.set(properties.zPassAsyncComputeThreadLimit.value);
    pixelAsyncComputeThreadLimit.set(properties.pixelAsyncComputeThreadLimit.value);
}

bool StateComputeModeProperties::isDirty() {
    return isCoherencyRequired.isDirty || largeGrfMode.isDirty || zPassAsyncComputeThreadLimit.isDirty ||
           pixelAsyncComputeThreadLimit.isDirty;
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
    largeGrfMode.isDirty = false;
    zPassAsyncComputeThreadLimit.isDirty = false;
    pixelAsyncComputeThreadLimit.isDirty = false;
}

void FrontEndProperties::setProperties(bool isCooperativeKernel, bool disableOverdispatch, int32_t engineInstancedDevice,
                                       const HardwareInfo &hwInfo) {
    clearIsDirty();

    this->disableOverdispatch.set(disableOverdispatch);
    this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
}

void FrontEndProperties::setProperties(const FrontEndProperties &properties) {
    clearIsDirty();

    disableOverdispatch.set(properties.disableOverdispatch.value);
    singleSliceDispatchCcsMode.set(properties.singleSliceDispatchCcsMode.value);
}

bool FrontEndProperties::isDirty() {
    return disableOverdispatch.isDirty || singleSliceDispatchCcsMode.isDirty;
}

void FrontEndProperties::clearIsDirty() {
    disableOverdispatch.isDirty = false;
    singleSliceDispatchCcsMode.isDirty = false;
}
