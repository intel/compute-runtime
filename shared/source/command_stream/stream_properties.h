/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

#include "stream_properties.inl"

namespace NEO {

struct StreamProperties {
    bool setCooperativeKernelProperties(int32_t cooperativeKernelProperties, const HardwareInfo &hwInfo);
    int32_t getCooperativeKernelProperties() const;

    void setStateComputeModeProperties(bool requiresCoherency, uint32_t numGrfRequired, bool isMultiOsContextCapable,
                                       bool useGlobalAtomics, bool areMultipleSubDevicesInContext);

    StateComputeModeProperties stateComputeMode{};
    FrontEndProperties frontEndState{};
};

} // namespace NEO
