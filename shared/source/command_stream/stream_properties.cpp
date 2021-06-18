/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

using namespace NEO;

void StateComputeModeProperties::setProperties(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy) {
    clearIsDirty();

    int32_t isCoherencyRequired = (requiresCoherency ? 1 : 0);
    this->isCoherencyRequired.set(isCoherencyRequired);
}

void StateComputeModeProperties::setProperties(const StateComputeModeProperties &properties) {
    clearIsDirty();

    isCoherencyRequired.set(properties.isCoherencyRequired.value);
}

bool StateComputeModeProperties::isDirty() {
    return isCoherencyRequired.isDirty;
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
}

void FrontEndProperties::setProperties(bool isCooperativeKernel, bool disableOverdispatch, const HardwareInfo &hwInfo) {
}

void FrontEndProperties::setProperties(const FrontEndProperties &properties) {
}

bool FrontEndProperties::isDirty() {
    return false;
}

void FrontEndProperties::clearIsDirty() {
}
