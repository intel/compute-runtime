/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

using namespace NEO;

void StateComputeModeProperties::setPropertiesExtra(bool reportNumGrf, bool reportThreadArbitrationPolicy) {
}
void StateComputeModeProperties::setPropertiesExtra(const StateComputeModeProperties &properties) {
}
bool StateComputeModeProperties::isDirtyExtra() const {
    return false;
}
void StateComputeModeProperties::clearIsDirtyExtra() {
}
