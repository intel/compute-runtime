/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

using namespace NEO;

bool StreamProperties::setCooperativeKernelProperties(int32_t cooperativeKernelProperties, const HardwareInfo &hwInfo) {
    return false;
}

int32_t StreamProperties::getCooperativeKernelProperties() const {
    return -1;
}

void StreamProperties::setStateComputeModeProperties(bool requiresCoherency, uint32_t numGrfRequired, bool isMultiOsContextCapable,
                                                     bool useGlobalAtomics, bool areMultipleSubDevicesInContext) {
    stateComputeMode.clearIsDirty();

    int32_t isCoherencyRequired = (requiresCoherency ? 1 : 0);
    stateComputeMode.isCoherencyRequired.set(isCoherencyRequired);
}

bool StateComputeModeProperties::isDirty() {
    return isCoherencyRequired.isDirty;
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
}
