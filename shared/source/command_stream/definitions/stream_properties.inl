/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_property.h"

namespace NEO {

struct StateComputeModePropertiesSupport {
    bool coherencyRequired = false;
    bool largeGrfMode = false;
    bool zPassAsyncComputeThreadLimit = false;
    bool pixelAsyncComputeThreadLimit = false;
    bool threadArbitrationPolicy = false;
    bool devicePreemptionMode = false;
};

struct StateComputeModeProperties {
    StreamProperty isCoherencyRequired{};
    StreamProperty largeGrfMode{};
    StreamProperty zPassAsyncComputeThreadLimit{};
    StreamProperty pixelAsyncComputeThreadLimit{};
    StreamProperty threadArbitrationPolicy{};
    StreamProperty devicePreemptionMode{};

    void setProperties(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, PreemptionMode devicePreemptionMode, const HardwareInfo &hwInfo);
    void setProperties(const StateComputeModeProperties &properties);
    bool isDirty() const;

  protected:
    void clearIsDirty();

    bool isDirtyExtra() const;
    void setPropertiesExtra();
    void setPropertiesExtra(const StateComputeModeProperties &properties);
    void clearIsDirtyExtra();

    StateComputeModePropertiesSupport scmPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

struct FrontEndPropertiesSupport {
    bool computeDispatchAllWalker = false;
    bool disableEuFusion = false;
    bool disableOverdispatch = false;
    bool singleSliceDispatchCcsMode = false;
};

struct FrontEndProperties {
    StreamProperty computeDispatchAllWalkerEnable{};
    StreamProperty disableEUFusion{};
    StreamProperty disableOverdispatch{};
    StreamProperty singleSliceDispatchCcsMode{};

    void setProperties(bool isCooperativeKernel, bool disableEUFusion, bool disableOverdispatch, int32_t engineInstancedDevice, const HardwareInfo &hwInfo);
    void setProperties(const FrontEndProperties &properties);
    void setPropertySingleSliceDispatchCcsMode(int32_t engineInstancedDevice, const HardwareInfo &hwInfo);
    bool isDirty() const;

  protected:
    void clearIsDirty();

    FrontEndPropertiesSupport frontEndPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

struct PipelineSelectPropertiesSupport {
    bool modeSelected = false;
    bool mediaSamplerDopClockGate = false;
    bool systolicMode = false;
};

struct PipelineSelectProperties {
    StreamProperty modeSelected{};
    StreamProperty mediaSamplerDopClockGate{};
    StreamProperty systolicMode{};

    void setProperties(bool modeSelected, bool mediaSamplerDopClockGate, bool systolicMode, const HardwareInfo &hwInfo);
    void setProperties(const PipelineSelectProperties &properties);
    bool isDirty() const;

  protected:
    void clearIsDirty();

    PipelineSelectPropertiesSupport pipelineSelectPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

} // namespace NEO
