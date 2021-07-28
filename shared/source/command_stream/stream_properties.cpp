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
}

void StateComputeModeProperties::setProperties(const StateComputeModeProperties &properties) {
    clearIsDirty();

    isCoherencyRequired.set(properties.isCoherencyRequired.value);
    largeGrfMode.set(properties.largeGrfMode.value);
}

bool StateComputeModeProperties::isDirty() {
    return isCoherencyRequired.isDirty || largeGrfMode.isDirty;
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
    largeGrfMode.isDirty = false;
}

void FrontEndProperties::setProperties(bool isCooperativeKernel, bool disableOverdispatch, const HardwareInfo &hwInfo) {
    clearIsDirty();

    this->disableOverdispatch.set(disableOverdispatch ? 1 : 0);
}

void FrontEndProperties::setProperties(const FrontEndProperties &properties) {
    clearIsDirty();

    disableOverdispatch.set(properties.disableOverdispatch.value);
}

bool FrontEndProperties::isDirty() {
    return disableOverdispatch.isDirty;
}

void FrontEndProperties::clearIsDirty() {
    disableOverdispatch.isDirty = false;
}
