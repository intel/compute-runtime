/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_from_xe_hpc_to_xe3.inl"
#include "shared/source/os_interface/product_helper_from_xe_hpg_to_xe3.inl"
#include "shared/source/os_interface/product_helper_xe2_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_hpc_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_hpg_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_lpg_and_later.inl"

#include "aubstream/product_family.h"

namespace NEO {

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

    propertiesSupport.enableL1FlushUavCoherencyMode = GfxProduct::StateComputeModeStateSupport::enableL1FlushUavCoherencyMode;
    if (debugManager.flags.EnableL1FlushUavCoherencyMode.get() != -1) {
        propertiesSupport.enableL1FlushUavCoherencyMode = !!debugManager.flags.EnableL1FlushUavCoherencyMode.get();
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
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
bool ProductHelperHw<gfxProduct>::isL1PolicyMissmatchCheckNeeded() const {
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

template <>
bool ProductHelperHw<gfxProduct>::isMisalignedUserPtr2WayCoherent() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::overrideAllocationCpuCacheable(const AllocationData &allocationData) const {
    return GraphicsAllocation::isAccessedFromCommandStreamer(allocationData.type);
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Ptl;
};

template <>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return true;
}

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    return GfxMemoryAllocationMethod::allocateByKmd;
}

template <>
bool ProductHelperHw<gfxProduct>::isStagingBuffersEnabled() const {
    return true;
}

} // namespace NEO
