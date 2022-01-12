/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"

#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

LinearStream::LinearStream(GraphicsAllocation *gfxAllocation, void *buffer, size_t bufferSize)
    : sizeUsed(0), maxAvailableSpace(bufferSize), buffer(buffer), graphicsAllocation(gfxAllocation) {
}

LinearStream::LinearStream(void *buffer, size_t bufferSize)
    : LinearStream(nullptr, buffer, bufferSize) {
}

LinearStream::LinearStream(GraphicsAllocation *gfxAllocation)
    : sizeUsed(0), graphicsAllocation(gfxAllocation) {
    if (gfxAllocation) {
        maxAvailableSpace = gfxAllocation->getUnderlyingBufferSize();
        buffer = gfxAllocation->getUnderlyingBuffer();
    } else {
        maxAvailableSpace = 0;
        buffer = nullptr;
    }
}

LinearStream::LinearStream()
    : LinearStream(nullptr) {
}

LinearStream::LinearStream(void *buffer, size_t bufferSize, CommandContainer *cmdContainer, size_t batchBufferEndSize)
    : LinearStream(buffer, bufferSize) {
    this->cmdContainer = cmdContainer;
    this->batchBufferEndSize = batchBufferEndSize;
}
} // namespace NEO
