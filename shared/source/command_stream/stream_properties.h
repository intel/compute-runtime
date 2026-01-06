/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "stream_properties.inl"

namespace NEO {

struct PipelineSelectPropertiesSupport {
    bool systolicMode = false;
};

struct PipelineSelectProperties {
    StreamProperty modeSelected{};
    StreamProperty systolicMode{};

    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);
    void resetState();

    void setPropertiesAll(bool modeSelected, bool systolicMode);
    void setPropertiesModeSelected(bool modeSelected, bool clearDirtyState);
    void setPropertySystolicMode(bool systolicMode);

    void copyPropertiesAll(const PipelineSelectProperties &properties);
    void copyPropertiesSystolicMode(const PipelineSelectProperties &properties);

    bool isDirty() const;
    void clearIsDirty();

  protected:
    PipelineSelectPropertiesSupport pipelineSelectPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

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
