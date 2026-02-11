/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_xe2_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_hpc_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_hpg_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_lpg_and_later.inl"

namespace NEO {

template <>
void ProductHelperHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    hwInfo->platform.eRenderCoreFamily = IGFX_XE3P_CORE;
}

template <>
bool ProductHelperHw<gfxProduct>::isBlitterForImagesSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isReleaseGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return !hwInfo.capabilityTable.isIntegratedDevice;
}

template <>
bool ProductHelperHw<gfxProduct>::isAcquireGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const {
    return !hwInfo.capabilityTable.isIntegratedDevice;
}

template <>
void ProductHelperHw<gfxProduct>::fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;

    fillScmPropertiesSupportStructureBase(propertiesSupport);
    propertiesSupport.allocationForScratchAndMidthreadPreemption = GfxProduct::StateComputeModeStateSupport::allocationForScratchAndMidthreadPreemption;

    propertiesSupport.enableVariableRegisterSizeAllocation = GfxProduct::StateComputeModeStateSupport::enableVariableRegisterSizeAllocation;
    if (debugManager.flags.EnableXe3VariableRegisterSizeAllocation.get() != -1) {
        propertiesSupport.enableVariableRegisterSizeAllocation = !!debugManager.flags.EnableXe3VariableRegisterSizeAllocation.get();
    }
    propertiesSupport.largeGrfMode = !propertiesSupport.enableVariableRegisterSizeAllocation;

    bool pipelinedEuThreadArbitration = true;
    if (debugManager.flags.PipelinedEuThreadArbitration.get() != -1) {
        pipelinedEuThreadArbitration = !!debugManager.flags.PipelinedEuThreadArbitration.get();
    }

    if (pipelinedEuThreadArbitration) {
        propertiesSupport.pipelinedEuThreadArbitration = true;
    }

    propertiesSupport.lscSamplerBackingThreshold = true;
    propertiesSupport.enableL1FlushUavCoherencyMode = GfxProduct::StateComputeModeStateSupport::enableL1FlushUavCoherencyMode;
    if (debugManager.flags.EnableL1FlushUavCoherencyMode.get() != -1) {
        propertiesSupport.enableL1FlushUavCoherencyMode = !!debugManager.flags.EnableL1FlushUavCoherencyMode.get();
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isTranslationExceptionSupported() const {
    return debugManager.flags.TemporaryEnableOutOfBoundariesInTranslationException.get();
}

template <>
void ProductHelperHw<gfxProduct>::fillScmPropertiesSupportStructureExtra(StateComputeModePropertiesSupport &propertiesSupport, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    bool debuggingEnabled = (rootDeviceEnvironment.debugger != nullptr);
    if (isTranslationExceptionSupported()) {
        propertiesSupport.enableOutOfBoundariesInTranslationException = debuggingEnabled;
    }
    propertiesSupport.enablePageFaultException = GfxProduct::StateComputeModeStateSupport::enablePageFaultException;
    propertiesSupport.enableMemoryException = GfxProduct::StateComputeModeStateSupport::enableMemoryException;
    propertiesSupport.enableBreakpoints = GfxProduct::StateComputeModeStateSupport::enableBreakpoints;
    propertiesSupport.enableForceExternalHaltAndForceException = GfxProduct::StateComputeModeStateSupport::enableForceExternalHaltAndForceException;

    if (debuggingEnabled) {
        if (debugManager.flags.TemporaryEnablePageFaultException.get()) {
            propertiesSupport.enablePageFaultException = true;
        }
        propertiesSupport.enableMemoryException = true;
        propertiesSupport.enableBreakpoints = true;
        propertiesSupport.enableForceExternalHaltAndForceException = true;
    }

    propertiesSupport.enableSystemMemoryReadFence = true;
}

template <>
bool ProductHelperHw<gfxProduct>::supports2DBlockLoad() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::supports2DBlockStore() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmAllocationReuseSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmAllocationReuseSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmPoolAllocatorSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmPoolAllocatorSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::initializeInternalEngineImmediately() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isFlushBetweenBlitsRequired() const {
    return false;
}

} // namespace NEO
