/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/indirect_heap/indirect_heap.h"

namespace NEO {

struct SBAPlaceholder {};
template <typename GfxFamily>
concept GfxFamilyWithSBA = requires() {
                               typename GfxFamily::STATE_BASE_ADDRESS;
                           };
template <typename GfxFamily>
struct StateBaseAddressTypeHelper;

template <typename GfxFamily>
struct StateBaseAddressTypeHelper {
    using type = SBAPlaceholder;
};

template <GfxFamilyWithSBA Family>
struct StateBaseAddressTypeHelper<Family> {
    using type = typename Family::STATE_BASE_ADDRESS;
};

inline uint64_t getStateBaseAddress(const IndirectHeap &heap, const bool useGlobalHeaps) {
    if (useGlobalHeaps) {
        return heap.getGraphicsAllocation()->getGpuBaseAddress();
    } else {
        return heap.getHeapGpuBase();
    }
}

inline size_t getStateSize(const IndirectHeap &heap, const bool useGlobalHeaps) {
    if (useGlobalHeaps) {
        return MemoryConstants::sizeOf4GBinPageEntities;
    } else {
        return heap.getHeapSizeInPages();
    }
}

inline uint64_t getStateBaseAddressForSsh(const IndirectHeap &heap, const bool useGlobalHeaps) {
    return heap.getHeapGpuBase();
}

inline size_t getStateSizeForSsh(const IndirectHeap &heap, const bool useGlobalHeaps) {
    return heap.getHeapSizeInPages();
}

} // namespace NEO
