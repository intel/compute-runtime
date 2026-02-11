/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_from_xe_hpc_to_xe3.inl"
#include "shared/source/os_interface/product_helper_from_xe_hpg_to_xe3.inl"
#include "shared/source/os_interface/product_helper_xe2_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_hpc_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_hpg_and_later.inl"
#include "shared/source/os_interface/product_helper_xe_lpg_and_later.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isBlitterForImagesSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isReleaseGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isAcquireGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const {
    return !hwInfo.capabilityTable.isIntegratedDevice;
}

template <>
bool ProductHelperHw<gfxProduct>::isGlobalFenceInPostSyncRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
void ProductHelperHw<gfxProduct>::fillScmPropertiesSupportStructure(StateComputeModePropertiesSupport &propertiesSupport) const {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;

    fillScmPropertiesSupportStructureBase(propertiesSupport);
    propertiesSupport.allocationForScratchAndMidthreadPreemption = GfxProduct::StateComputeModeStateSupport::allocationForScratchAndMidthreadPreemption;
    propertiesSupport.enableL1FlushUavCoherencyMode = GfxProduct::StateComputeModeStateSupport::enableL1FlushUavCoherencyMode;
    if (debugManager.flags.EnableL1FlushUavCoherencyMode.get() != -1) {
        propertiesSupport.enableL1FlushUavCoherencyMode = !!debugManager.flags.EnableL1FlushUavCoherencyMode.get();
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isStagingBuffersEnabled() const {
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

} // namespace NEO
