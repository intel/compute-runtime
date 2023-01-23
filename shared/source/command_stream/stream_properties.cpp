/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/hw_info_config.h"

using namespace NEO;

void StateComputeModeProperties::setProperties(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, PreemptionMode devicePreemptionMode,
                                               const RootDeviceEnvironment &rootDeviceEnvironment) {

    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        productHelper.fillScmPropertiesSupportStructure(this->scmPropertiesSupport);
        this->propertiesSupportLoaded = true;
    }

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    clearIsDirty();

    if (this->scmPropertiesSupport.coherencyRequired) {
        int32_t isCoherencyRequired = (requiresCoherency ? 1 : 0);
        this->isCoherencyRequired.set(isCoherencyRequired);
    }

    if (this->scmPropertiesSupport.largeGrfMode &&
        (this->largeGrfMode.value == -1 || numGrfRequired != GrfConfig::NotApplicable)) {
        int32_t largeGrfMode = (numGrfRequired == GrfConfig::LargeGrfNumber ? 1 : 0);
        this->largeGrfMode.set(largeGrfMode);
    }

    int32_t zPassAsyncComputeThreadLimit = -1;
    if (DebugManager.flags.ForceZPassAsyncComputeThreadLimit.get() != -1) {
        zPassAsyncComputeThreadLimit = DebugManager.flags.ForceZPassAsyncComputeThreadLimit.get();
    }
    if (zPassAsyncComputeThreadLimit != -1 && this->scmPropertiesSupport.zPassAsyncComputeThreadLimit) {
        this->zPassAsyncComputeThreadLimit.set(zPassAsyncComputeThreadLimit);
    }

    int32_t pixelAsyncComputeThreadLimit = -1;
    if (DebugManager.flags.ForcePixelAsyncComputeThreadLimit.get() != -1) {
        pixelAsyncComputeThreadLimit = DebugManager.flags.ForcePixelAsyncComputeThreadLimit.get();
    }
    if (pixelAsyncComputeThreadLimit != -1 && this->scmPropertiesSupport.pixelAsyncComputeThreadLimit) {
        this->pixelAsyncComputeThreadLimit.set(pixelAsyncComputeThreadLimit);
    }

    bool setDefaultThreadArbitrationPolicy = (threadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent) &&
                                             (NEO::DebugManager.flags.ForceDefaultThreadArbitrationPolicyIfNotSpecified.get() ||
                                              (this->threadArbitrationPolicy.value == ThreadArbitrationPolicy::NotPresent));
    if (setDefaultThreadArbitrationPolicy) {
        threadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    }
    if (DebugManager.flags.OverrideThreadArbitrationPolicy.get() != -1) {
        threadArbitrationPolicy = DebugManager.flags.OverrideThreadArbitrationPolicy.get();
    }
    if (this->scmPropertiesSupport.threadArbitrationPolicy) {
        this->threadArbitrationPolicy.set(threadArbitrationPolicy);
    }

    if (this->scmPropertiesSupport.devicePreemptionMode) {
        this->devicePreemptionMode.set(static_cast<int32_t>(devicePreemptionMode));
    }

    setPropertiesExtra();
}

void StateComputeModeProperties::setProperties(const StateComputeModeProperties &properties) {
    clearIsDirty();

    isCoherencyRequired.set(properties.isCoherencyRequired.value);
    largeGrfMode.set(properties.largeGrfMode.value);
    zPassAsyncComputeThreadLimit.set(properties.zPassAsyncComputeThreadLimit.value);
    pixelAsyncComputeThreadLimit.set(properties.pixelAsyncComputeThreadLimit.value);
    threadArbitrationPolicy.set(properties.threadArbitrationPolicy.value);
    devicePreemptionMode.set(properties.devicePreemptionMode.value);

    setPropertiesExtra(properties);
}

bool StateComputeModeProperties::isDirty() const {
    return isCoherencyRequired.isDirty || largeGrfMode.isDirty || zPassAsyncComputeThreadLimit.isDirty ||
           pixelAsyncComputeThreadLimit.isDirty || threadArbitrationPolicy.isDirty || devicePreemptionMode.isDirty || isDirtyExtra();
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
    largeGrfMode.isDirty = false;
    zPassAsyncComputeThreadLimit.isDirty = false;
    pixelAsyncComputeThreadLimit.isDirty = false;
    threadArbitrationPolicy.isDirty = false;
    devicePreemptionMode.isDirty = false;

    clearIsDirtyExtra();
}

void FrontEndProperties::setProperties(bool isCooperativeKernel, bool disableEUFusion, bool disableOverdispatch, int32_t engineInstancedDevice, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {

        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        productHelper.fillFrontEndPropertiesSupportStructure(this->frontEndPropertiesSupport, hwInfo);
        this->propertiesSupportLoaded = true;
    }

    clearIsDirty();

    if (this->frontEndPropertiesSupport.computeDispatchAllWalker) {
        this->computeDispatchAllWalkerEnable.set(isCooperativeKernel);
    }

    if (this->frontEndPropertiesSupport.disableEuFusion) {
        this->disableEUFusion.set(disableEUFusion);
    }

    if (this->frontEndPropertiesSupport.disableOverdispatch) {
        this->disableOverdispatch.set(disableOverdispatch);
    }

    if (this->frontEndPropertiesSupport.singleSliceDispatchCcsMode) {
        this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
    }
}

void FrontEndProperties::setPropertySingleSliceDispatchCcsMode(int32_t engineInstancedDevice, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        productHelper.fillFrontEndPropertiesSupportStructure(this->frontEndPropertiesSupport, hwInfo);
        this->propertiesSupportLoaded = true;
    }
    this->singleSliceDispatchCcsMode.isDirty = false;
    if (this->frontEndPropertiesSupport.singleSliceDispatchCcsMode) {
        this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
    }
}

void FrontEndProperties::setProperties(const FrontEndProperties &properties) {
    clearIsDirty();

    disableOverdispatch.set(properties.disableOverdispatch.value);
    disableEUFusion.set(properties.disableEUFusion.value);
    singleSliceDispatchCcsMode.set(properties.singleSliceDispatchCcsMode.value);
    computeDispatchAllWalkerEnable.set(properties.computeDispatchAllWalkerEnable.value);
}

bool FrontEndProperties::isDirty() const {
    return disableOverdispatch.isDirty || disableEUFusion.isDirty || singleSliceDispatchCcsMode.isDirty ||
           computeDispatchAllWalkerEnable.isDirty;
}

void FrontEndProperties::clearIsDirty() {
    disableEUFusion.isDirty = false;
    disableOverdispatch.isDirty = false;
    singleSliceDispatchCcsMode.isDirty = false;
    computeDispatchAllWalkerEnable.isDirty = false;
}

void PipelineSelectProperties::setProperties(bool modeSelected, bool mediaSamplerDopClockGate, bool systolicMode, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

        productHelper.fillPipelineSelectPropertiesSupportStructure(this->pipelineSelectPropertiesSupport, *rootDeviceEnvironment.getHardwareInfo());
        this->propertiesSupportLoaded = true;
    }

    clearIsDirty();

    if (this->pipelineSelectPropertiesSupport.modeSelected) {
        this->modeSelected.set(modeSelected);
    }

    if (this->pipelineSelectPropertiesSupport.mediaSamplerDopClockGate) {
        this->mediaSamplerDopClockGate.set(mediaSamplerDopClockGate);
    }

    if (this->pipelineSelectPropertiesSupport.systolicMode) {
        this->systolicMode.set(systolicMode);
    }
}

void PipelineSelectProperties::setProperties(const PipelineSelectProperties &properties) {
    clearIsDirty();

    modeSelected.set(properties.modeSelected.value);
    mediaSamplerDopClockGate.set(properties.mediaSamplerDopClockGate.value);
    systolicMode.set(properties.systolicMode.value);
}

bool PipelineSelectProperties::isDirty() const {
    return modeSelected.isDirty || mediaSamplerDopClockGate.isDirty || systolicMode.isDirty;
}

void PipelineSelectProperties::clearIsDirty() {
    modeSelected.isDirty = false;
    mediaSamplerDopClockGate.isDirty = false;
    systolicMode.isDirty = false;
}

void StateBaseAddressProperties::setProperties(bool globalAtomics, int32_t statelessMocs, int64_t bindingTablePoolBaseAddress,
                                               int64_t surfaceStateBaseAddress, size_t surfaceStateSize,
                                               int64_t dynamicStateBaseAddress, size_t dynamicStateSize,
                                               int64_t indirectObjectBaseAddress, size_t indirectObjectSize, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        productHelper.fillStateBaseAddressPropertiesSupportStructure(this->stateBaseAddressPropertiesSupport);
        this->propertiesSupportLoaded = true;
    }

    clearIsDirty();

    if (this->stateBaseAddressPropertiesSupport.globalAtomics) {
        this->globalAtomics.set(globalAtomics);
    }

    if (this->stateBaseAddressPropertiesSupport.statelessMocs) {
        this->statelessMocs.set(statelessMocs);
    }

    if (this->stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress) {
        this->bindingTablePoolBaseAddress.set(bindingTablePoolBaseAddress);
    }

    this->surfaceStateBaseAddress.set(surfaceStateBaseAddress);
    this->surfaceStateSize.set(surfaceStateSize);
    this->dynamicStateBaseAddress.set(dynamicStateBaseAddress);
    this->dynamicStateSize.set(dynamicStateSize);
    this->indirectObjectBaseAddress.set(indirectObjectBaseAddress);
    this->indirectObjectSize.set(indirectObjectSize);
}

void StateBaseAddressProperties::setProperties(const StateBaseAddressProperties &properties) {
    clearIsDirty();

    globalAtomics.set(properties.globalAtomics.value);
    statelessMocs.set(properties.statelessMocs.value);
    bindingTablePoolBaseAddress.set(properties.bindingTablePoolBaseAddress.value);

    surfaceStateBaseAddress.set(properties.surfaceStateBaseAddress.value);
    surfaceStateSize.set(properties.surfaceStateSize.value);
    dynamicStateBaseAddress.set(properties.dynamicStateBaseAddress.value);
    dynamicStateSize.set(properties.dynamicStateSize.value);
    indirectObjectBaseAddress.set(properties.indirectObjectBaseAddress.value);
    indirectObjectSize.set(properties.indirectObjectSize.value);
}

bool StateBaseAddressProperties::isDirty() const {
    return globalAtomics.isDirty || statelessMocs.isDirty || bindingTablePoolBaseAddress.isDirty ||
           surfaceStateBaseAddress.isDirty || surfaceStateSize.isDirty ||
           dynamicStateBaseAddress.isDirty || dynamicStateSize.isDirty ||
           indirectObjectBaseAddress.isDirty || indirectObjectSize.isDirty;
}

void StateBaseAddressProperties::clearIsDirty() {
    globalAtomics.isDirty = false;
    statelessMocs.isDirty = false;
    bindingTablePoolBaseAddress.isDirty = false;
    surfaceStateBaseAddress.isDirty = false;
    surfaceStateSize.isDirty = false;
    dynamicStateBaseAddress.isDirty = false;
    dynamicStateSize.isDirty = false;
    indirectObjectBaseAddress.isDirty = false;
    indirectObjectSize.isDirty = false;
}
