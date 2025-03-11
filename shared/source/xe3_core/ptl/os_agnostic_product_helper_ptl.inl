/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/product_family.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::overrideAllocationCpuCacheable(const AllocationData &allocationData) const {
    return allocationData.type == AllocationType::commandBuffer || allocationData.type == AllocationType::timestampPacketTagBuffer || this->overrideCacheableForDcFlushMitigation(allocationData.type);
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Ptl;
};

template <>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return false;
}

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    return GfxMemoryAllocationMethod::allocateByKmd;
}

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const {
    return true;
}

} // namespace NEO
