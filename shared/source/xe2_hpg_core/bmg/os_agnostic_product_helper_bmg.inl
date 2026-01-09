/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/memory_manager/allocation_type.h"

#include "aubstream/product_family.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported() const {
    if (debugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return debugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return false;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Bmg;
};

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    if constexpr (is32bit) {
        if (allocationType == AllocationType::svmCpu) { // no heap SVM in allocateByKmd on 32 bit
            return GfxMemoryAllocationMethod::useUmdSystemPtr;
        }
    }
    return GfxMemoryAllocationMethod::allocateByKmd;
}

template <>
void ProductHelperHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
}

template <>
bool ProductHelperHw<gfxProduct>::adjustDispatchAllRequired(const HardwareInfo &hwInfo) const {
    return hwInfo.gtSystemInfo.SliceCount > 2u;
}

template <>
void ProductHelperHw<gfxProduct>::adjustScratchSize(size_t &requiredScratchSize) const {
    requiredScratchSize *= 2;
}

template <>
bool ProductHelperHw<gfxProduct>::checkBcsForDirectSubmissionStop() const {
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
bool ProductHelperHw<gfxProduct>::isPoolManagerEnabled() const {
    return false;
}

} // namespace NEO
