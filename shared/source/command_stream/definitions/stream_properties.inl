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
struct RootDeviceEnvironment;

struct StateComputeModePropertiesSupport {
    bool coherencyRequired : 1 = false;
    bool largeGrfMode : 1 = false;
    bool zPassAsyncComputeThreadLimit : 1 = false;
    bool pixelAsyncComputeThreadLimit : 1 = false;
    bool threadArbitrationPolicy : 1 = false;
    bool devicePreemptionMode : 1 = false;
    bool allocationForScratchAndMidthreadPreemption : 1 = false;
    bool enableVariableRegisterSizeAllocation : 1 = false;
    bool pipelinedEuThreadArbitration : 1 = false;
    bool enableL1FlushUavCoherencyMode : 1 = false;
    bool lscSamplerBackingThreshold : 1 = false;
    bool enableOutOfBoundariesInTranslationException : 1 = false;
    bool enablePageFaultException : 1 = false;
    bool enableSystemMemoryReadFence : 1 = false;
    bool enableMemoryException : 1 = false;
    bool enableBreakpoints : 1 = false;
    bool enableForceExternalHaltAndForceException : 1 = false;
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
    StreamProperty pipelinedEuThreadArbitration{};
    StreamProperty enableL1FlushUavCoherencyMode{};
    StreamProperty lscSamplerBackingThreshold{};
    StreamProperty enableOutOfBoundariesInTranslationException{};
    StreamProperty enablePageFaultException{};
    StreamProperty enableSystemMemoryReadFence{};
    StreamProperty enableMemoryException{};
    StreamProperty enableBreakpoints{};
    StreamProperty enableForceExternalHaltAndForceException{};

    void initSupport(const RootDeviceEnvironment &rootDeviceEnvironment);
    void resetState();

    void setPropertiesAll(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, PreemptionMode devicePreemptionMode, std::optional<bool> hasPeerAccess);
    void setPropertiesPerContext(bool requiresCoherency, PreemptionMode devicePreemptionMode, bool clearDirtyState, std::optional<bool> hasPeerAccess);
    void setPropertiesGrfNumberThreadArbitration(uint32_t numGrfRequired, int32_t threadArbitrationPolicy);

    void copyPropertiesAll(const StateComputeModeProperties &properties);
    void copyPropertiesGrfNumberThreadArbitration(const StateComputeModeProperties &properties);

    bool isDirty() const;
    void clearIsDirty();

    bool isPipelinedEuThreadArbitrationEnabled() const {
        return this->scmPropertiesSupport.pipelinedEuThreadArbitration;
    }

  protected:
    void clearIsDirtyPerContext();
    void clearIsDirtyExtraPerContext();
    bool isDirtyExtra() const;
    void resetStateExtra();

    void setPropertiesExtraPerContext();

    void copyPropertiesExtra(const StateComputeModeProperties &properties);

    void setCoherencyProperty(bool requiresCoherency);
    void setDevicePreemptionProperty(PreemptionMode devicePreemptionMode);
    void setGrfNumberProperty(uint32_t numGrfRequired);
    void setThreadArbitrationProperty(int32_t threadArbitrationPolicy);

    StateComputeModePropertiesSupport scmPropertiesSupport = {};
    int32_t defaultThreadArbitrationPolicy = 0;
    bool propertiesSupportLoaded = false;
};

struct FrontEndPropertiesSupport {
    bool computeDispatchAllWalker : 1 = false;
    bool disableEuFusion : 1 = false;
    bool disableOverdispatch : 1 = false;
    bool singleSliceDispatchCcsMode : 1 = false;
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
