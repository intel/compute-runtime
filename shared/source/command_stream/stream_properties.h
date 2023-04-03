/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "stream_properties.inl"

namespace NEO {

struct StreamProperties {
    StateComputeModeProperties stateComputeMode{};
    FrontEndProperties frontEndState{};
    PipelineSelectProperties pipelineSelect{};
    StateBaseAddressProperties stateBaseAddress{};

    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment) {
        stateComputeMode.initSupport(rootDeviceEnvironment);
        frontEndState.initSupport(rootDeviceEnvironment);
        pipelineSelect.initSupport(rootDeviceEnvironment);
        stateBaseAddress.initSupport(rootDeviceEnvironment);
    }
    void resetState() {
        stateComputeMode.resetState();
        frontEndState.resetState();
        pipelineSelect.resetState();
        stateBaseAddress.resetState();
    }
};

} // namespace NEO
