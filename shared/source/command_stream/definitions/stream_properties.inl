/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_property.h"

#include <optional>

namespace NEO {
enum PreemptionMode : uint32_t;
struct HardwareInfo;
struct RootDeviceEnvironment;

struct StateComputeModePropertiesSupport {
    bool coherencyRequired = false;
    bool largeGrfMode = false;
    bool zPassAsyncComputeThreadLimit = false;
    bool pixelAsyncComputeThreadLimit = false;
    bool threadArbitrationPolicy = false;
    bool devicePreemptionMode = false;
    bool allocationForScratchAndMidthreadPreemption = false;
    bool enableVariableRegisterSizeAllocation = false;
    bool pipelinedEuThreadArbitration = false;
};

struct StateComputeModeProperties {
    StreamProperty isCoherencyRequired{};
    StreamProperty largeGrfMode{};
    StreamProperty zPassAsyncComputeThreadLimit{};
    StreamProperty pixelAsyncComputeThreadLimit{};
    StreamProperty threadArbitrationPolicy{};
    StreamProperty devicePreemptionMode{};
    StreamProperty memoryAllocationForScratchAndMidthreadPreemptionBuffers{};
    StreamProperty enableVariableRegisterSizeAllocation{};

    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);
    void resetState();

    void setPropertiesAll(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, PreemptionMode devicePreemptionMode, std::optional<bool> hasPeerAccess);
    void setPropertiesPerContext(bool requiresCoherency, PreemptionMode devicePreemptionMode, bool clearDirtyState, std::optional<bool> hasPeerAccess);
    void setPropertiesGrfNumberThreadArbitration(uint32_t numGrfRequired, int32_t threadArbitrationPolicy);

    void copyPropertiesAll(const StateComputeModeProperties &properties);
    void copyPropertiesGrfNumberThreadArbitration(const StateComputeModeProperties &properties);
    void setPipelinedEuThreadArbitration();
    bool isPipelinedEuThreadArbitrationEnabled() const;

    bool isDirty() const;
    void clearIsDirty();

  protected:
    void clearIsDirtyPerContext();
    void clearIsDirtyExtraPerContext();
    bool isDirtyExtra() const;
    void resetStateExtra();

    void setPropertiesExtraPerContext(std::optional<bool> hasPeerAccess);

    void copyPropertiesExtra(const StateComputeModeProperties &properties);

    void setCoherencyProperty(bool requiresCoherency);
    void setDevicePreemptionProperty(PreemptionMode devicePreemptionMode);
    void setGrfNumberProperty(uint32_t numGrfRequired);
    void setThreadArbitrationProperty(int32_t threadArbitrationPolicy);

    StateComputeModePropertiesSupport scmPropertiesSupport = {};
    int32_t defaultThreadArbitrationPolicy = 0;
    bool propertiesSupportLoaded = false;
    bool pipelinedEuThreadArbitration = false;
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

    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);
    void resetState();

    void setPropertiesAll(bool isCooperativeKernel, bool disableEuFusion, bool disableOverdispatch);
    void setPropertySingleSliceDispatchCcsMode();
    void setPropertiesDisableOverdispatch(bool disableOverdispatch, bool clearDirtyState);
    void setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(bool isCooperativeKernel, bool disableEuFusion);

    void copyPropertiesAll(const FrontEndProperties &properties);
    void copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(const FrontEndProperties &properties);

    bool isDirty() const;
    void clearIsDirty();

  protected:
    FrontEndPropertiesSupport frontEndPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

struct StateBaseAddressPropertiesSupport {
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
    StreamProperty statelessMocs{};

    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);
    void resetState();

    void setPropertiesAll(int32_t statelessMocs,
                          int64_t bindingTablePoolBaseAddress, size_t bindingTablePoolSize,
                          int64_t surfaceStateBaseAddress, size_t surfaceStateSize,
                          int64_t dynamicStateBaseAddress, size_t dynamicStateSize,
                          int64_t indirectObjectBaseAddress, size_t indirectObjectSize);
    void setPropertiesBindingTableSurfaceState(int64_t bindingTablePoolBaseAddress, size_t bindingTablePoolSize,
                                               int64_t surfaceStateBaseAddress, size_t surfaceStateSize);
    void setPropertiesSurfaceState(int64_t surfaceStateBaseAddress, size_t surfaceStateSize);
    void setPropertiesDynamicState(int64_t dynamicStateBaseAddress, size_t dynamicStateSize);
    void setPropertiesIndirectState(int64_t indirectObjectBaseAddress, size_t indirectObjectSize);
    void setPropertyStatelessMocs(int32_t statelessMocs);

    void copyPropertiesAll(const StateBaseAddressProperties &properties);
    void copyPropertiesStatelessMocs(const StateBaseAddressProperties &properties);
    void copyPropertiesStatelessMocsIndirectState(const StateBaseAddressProperties &properties);
    void copyPropertiesBindingTableSurfaceState(const StateBaseAddressProperties &properties);
    void copyPropertiesSurfaceState(const StateBaseAddressProperties &properties);
    void copyPropertiesDynamicState(const StateBaseAddressProperties &properties);

    bool isDirty() const;
    void clearIsDirty();

  protected:
    StateBaseAddressPropertiesSupport stateBaseAddressPropertiesSupport = {};
    bool propertiesSupportLoaded = false;
};

} // namespace NEO
