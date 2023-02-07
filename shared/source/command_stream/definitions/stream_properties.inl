/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_property.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
struct RootDeviceEnvironment;

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

    void setProperties(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, PreemptionMode devicePreemptionMode, const RootDeviceEnvironment &rootDeviceEnvironment);
    void setProperties(const StateComputeModeProperties &properties);
    void setPropertiesGrfNumberThreadArbitration(uint32_t numGrfRequired, int32_t threadArbitrationPolicy, const RootDeviceEnvironment &rootDeviceEnvironment);
    void setPropertiesCoherencyDevicePreemption(bool requiresCoherency, PreemptionMode devicePreemptionMode, const RootDeviceEnvironment &rootDeviceEnvironment);
    bool isDirty() const;

  protected:
    void clearIsDirty();

    bool isDirtyExtra() const;
    void setPropertiesExtra();
    void setPropertiesExtra(const StateComputeModeProperties &properties);
    void clearIsDirtyExtra();
    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);

    void setCoherencyProperty(bool requiresCoherency);
    void setDevicePreemptionProperty(PreemptionMode devicePreemptionMode);
    void setGrfNumberProperty(uint32_t numGrfRequired);
    void setThreadArbitrationProperty(int32_t threadArbitrationPolicy,
                                      const RootDeviceEnvironment &rootDeviceEnvironment);

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

    void setProperties(bool isCooperativeKernel, bool disableEuFusion, bool disableOverdispatch, int32_t engineInstancedDevice, const RootDeviceEnvironment &rootDeviceEnvironment);
    void setProperties(const FrontEndProperties &properties);
    void setPropertySingleSliceDispatchCcsMode(int32_t engineInstancedDevice, const RootDeviceEnvironment &rootDeviceEnvironment);
    void setPropertyDisableOverdispatch(bool disableOverdispatch, const RootDeviceEnvironment &rootDeviceEnvironment);
    void setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(bool isCooperativeKernel, bool disableEuFusion, const RootDeviceEnvironment &rootDeviceEnvironment);
    bool isDirty() const;

  protected:
    void clearIsDirty();
    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);

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

    void setProperties(bool modeSelected, bool mediaSamplerDopClockGate, bool systolicMode, const RootDeviceEnvironment &rootDeviceEnvironment);
    void setProperties(const PipelineSelectProperties &properties);
    void setPropertySystolicMode(bool systolicMode, const RootDeviceEnvironment &rootDeviceEnvironment);
    bool isDirty() const;

  protected:
    void clearIsDirty();
    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);

    PipelineSelectPropertiesSupport pipelineSelectPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

struct StateBaseAddressPropertiesSupport {
    bool globalAtomics = false;
    bool statelessMocs = false;
    bool bindingTablePoolBaseAddress = false;
};

struct StateBaseAddressProperties {
    StreamProperty64 bindingTablePoolBaseAddress{};
    StreamProperty64 surfaceStateBaseAddress{};
    StreamProperty64 dynamicStateBaseAddress{};
    StreamProperty64 indirectObjectBaseAddress{};
    StreamPropertySizeT bindingTablePoolSize{};
    StreamPropertySizeT surfaceStateSize{};
    StreamPropertySizeT dynamicStateSize{};
    StreamPropertySizeT indirectObjectSize{};
    StreamProperty globalAtomics{};
    StreamProperty statelessMocs{};

    void setProperties(bool globalAtomics, int32_t statelessMocs,
                       int64_t bindingTablePoolBaseAddress, size_t surfaceStateSize,
                       int64_t surfaceStateBaseAddress, size_t bindingTablePoolSize,
                       int64_t dynamicStateBaseAddress, size_t dynamicStateSize,
                       int64_t indirectObjectBaseAddress, size_t indirectObjectSize, const RootDeviceEnvironment &rootDeviceEnvironment);
    void setProperties(const StateBaseAddressProperties &properties);
    bool isDirty() const;

  protected:
    void clearIsDirty();

    StateBaseAddressPropertiesSupport stateBaseAddressPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

} // namespace NEO
