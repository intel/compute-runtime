/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace NEO {
class GraphicsAllocation;

class LinearStream {
  public:
    virtual ~LinearStream() = default;
    LinearStream();
    LinearStream(void *buffer, size_t bufferSize);
    LinearStream(GraphicsAllocation *buffer);
    LinearStream(GraphicsAllocation *gfxAllocation, void *buffer, size_t bufferSize);
    LinearStream(void *buffer, size_t bufferSize, CommandContainer *cmdContainer, size_t batchBufferEndSize);
    void *getCpuBase() const;
    void *getSpace(size_t size);
    size_t getMaxAvailableSpace() const;
    size_t getAvailableSpace() const;
    size_t getUsed() const;

    uint64_t getGpuBase() const;
    void setGpuBase(uint64_t);

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
    CommandContainer *cmdContainer = nullptr;
    size_t batchBufferEndSize = 0;
    uint64_t gpuBase = 0;
};

inline void *LinearStream::getCpuBase() const {
    return buffer;
}

inline void LinearStream::setGpuBase(uint64_t gpuAddress) {
    gpuBase = gpuAddress;
}

inline void *LinearStream::getSpace(size_t size) {
    if (cmdContainer != nullptr && getAvailableSpace() < batchBufferEndSize + size) {
        UNRECOVERABLE_IF(sizeUsed + batchBufferEndSize > maxAvailableSpace);
        cmdContainer->closeAndAllocateNextCommandBuffer();
    }
    UNRECOVERABLE_IF(sizeUsed + size > maxAvailableSpace);
    UNRECOVERABLE_IF(reinterpret_cast<int64_t>(buffer) <= 0);
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
} // namespace NEO
