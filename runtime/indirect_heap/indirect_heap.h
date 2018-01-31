/*
 * Copyright (c) 2017, Intel Corporation
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
        INSTRUCTION,
        SURFACE_STATE,
        NUM_TYPES
    };

    IndirectHeap(void *buffer, size_t bufferSize);
    IndirectHeap(GraphicsAllocation *buffer);

    // Disallow copy'ing
    IndirectHeap(const IndirectHeap &) = delete;
    IndirectHeap &operator=(const IndirectHeap &) = delete;

    void align(size_t alignment);
};

inline void IndirectHeap::align(size_t alignment) {
    auto address = alignUp(ptrOffset(buffer, sizeUsed), alignment);
    sizeUsed = ptrDiff(address, buffer);
}
}
