/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
class GraphicsAllocation;

using HeapContainer = std::vector<GraphicsAllocation *>;

constexpr size_t defaultHeapSize = 64 * KB;

inline size_t getDefaultHeapSize() {
    auto defaultSize = defaultHeapSize;
    if (DebugManager.flags.ForceDefaultHeapSize.get() != -1) {
        defaultSize = DebugManager.flags.ForceDefaultHeapSize.get() * MemoryConstants::kiloByte;
    }
    return defaultSize;
}

class IndirectHeap : public LinearStream {
    typedef LinearStream BaseClass;

  public:
    using Type = IndirectHeapType;
    IndirectHeap(void *graphicsAllocation, size_t bufferSize) : BaseClass(graphicsAllocation, bufferSize){};
    IndirectHeap(GraphicsAllocation *graphicsAllocation) : BaseClass(graphicsAllocation) {}
    IndirectHeap(GraphicsAllocation *graphicsAllocation, bool canBeUtilizedAs4GbHeap)
        : BaseClass(graphicsAllocation), canBeUtilizedAs4GbHeap(canBeUtilizedAs4GbHeap) {}

    // Disallow copy'ing
    IndirectHeap(const IndirectHeap &) = delete;
    IndirectHeap &operator=(const IndirectHeap &) = delete;

    void align(size_t alignment);
    uint64_t getHeapGpuStartOffset() const;
    uint64_t getHeapGpuBase() const;
    uint32_t getHeapSizeInPages() const;

  protected:
    bool canBeUtilizedAs4GbHeap = false;
};

inline void IndirectHeap::align(size_t alignment) {
    auto address = alignUp(ptrOffset(buffer, sizeUsed), alignment);
    sizeUsed = ptrDiff(address, buffer);
}

inline uint32_t IndirectHeap::getHeapSizeInPages() const {
    if (this->canBeUtilizedAs4GbHeap) {
        return MemoryConstants::sizeOf4GBinPageEntities;
    } else {
        return (static_cast<uint32_t>(getMaxAvailableSpace()) + MemoryConstants::pageMask) / MemoryConstants::pageSize;
    }
}

inline uint64_t IndirectHeap::getHeapGpuStartOffset() const {
    if (this->canBeUtilizedAs4GbHeap) {
        return this->graphicsAllocation->getGpuAddressToPatch();
    } else {
        return 0llu;
    }
}

inline uint64_t IndirectHeap::getHeapGpuBase() const {
    if (this->canBeUtilizedAs4GbHeap) {
        return this->graphicsAllocation->getGpuBaseAddress();
    } else {
        return this->graphicsAllocation->getGpuAddress();
    }
}
} // namespace NEO
