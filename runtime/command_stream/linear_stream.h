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
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include <cstddef>
#include <cstdint>
#include <atomic>

namespace OCLRT {
class GraphicsAllocation;

class LinearStream {
  public:
    virtual ~LinearStream() = default;
    LinearStream();
    LinearStream(void *buffer, size_t bufferSize);
    LinearStream(GraphicsAllocation *buffer);
    void *getCpuBase() const;
    void *getSpace(size_t size);
    size_t getMaxAvailableSpace() const;
    size_t getAvailableSpace() const;
    size_t getUsed() const;
    void overrideMaxSize(size_t newMaxSize);
    void replaceBuffer(void *buffer, size_t bufferSize);
    GraphicsAllocation *getGraphicsAllocation() const;
    void replaceGraphicsAllocation(GraphicsAllocation *gfxAllocation);

    template <typename Cmd>
    Cmd *getSpaceForCmd() {
        auto ptr = getSpace(sizeof(Cmd));
        return reinterpret_cast<Cmd *>(ptr);
    }

  protected:
    std::atomic<size_t> sizeUsed;
    size_t maxAvailableSpace;
    void *buffer;
    GraphicsAllocation *graphicsAllocation;
};

inline void *LinearStream::getCpuBase() const {
    return buffer;
}

inline void *LinearStream::getSpace(size_t size) {
    UNRECOVERABLE_IF(sizeUsed + size > maxAvailableSpace);
    auto memory = ptrOffset(buffer, sizeUsed);
    sizeUsed += size;
    return memory;
}

inline size_t LinearStream::getMaxAvailableSpace() const {
    return maxAvailableSpace;
}

inline size_t LinearStream::getAvailableSpace() const {
    DEBUG_BREAK_IF(sizeUsed > maxAvailableSpace);
    return maxAvailableSpace - sizeUsed;
}

inline size_t LinearStream::getUsed() const {
    return sizeUsed;
}

inline void LinearStream::overrideMaxSize(size_t newMaxSize) {
    maxAvailableSpace = newMaxSize;
}

inline void LinearStream::replaceBuffer(void *buffer, size_t bufferSize) {
    this->buffer = buffer;
    maxAvailableSpace = bufferSize;
    sizeUsed = 0;
}

inline GraphicsAllocation *LinearStream::getGraphicsAllocation() const {
    return graphicsAllocation;
}

inline void LinearStream::replaceGraphicsAllocation(GraphicsAllocation *gfxAllocation) {
    graphicsAllocation = gfxAllocation;
}
} // namespace OCLRT
