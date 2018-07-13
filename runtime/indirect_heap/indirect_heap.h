/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {
class GraphicsAllocation;

constexpr size_t defaultHeapSize = 64 * KB;
constexpr size_t maxSshSize = defaultHeapSize - MemoryConstants::pageSize;

class IndirectHeap : public LinearStream {
    typedef LinearStream BaseClass;

  public:
    enum Type {
        DYNAMIC_STATE = 0,
        GENERAL_STATE,
        INDIRECT_OBJECT,
        SURFACE_STATE,
        NUM_TYPES
    };

    IndirectHeap(void *buffer, size_t bufferSize);
    IndirectHeap(GraphicsAllocation *buffer);
    IndirectHeap(GraphicsAllocation *buffer, bool canBeUtilizedAs4GbHeap);

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
        return this->graphicsAllocation->gpuBaseAddress;
    } else {
        return this->graphicsAllocation->getGpuAddress();
    }
}
} // namespace OCLRT
