/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_xe_hpc_and_later.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isBlitterForImagesSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return true;
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
}

template <>
bool ProductHelperHw<gfxProduct>::isNewCoherencyModelSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
    return true;
}
} // namespace NEO
