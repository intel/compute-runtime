/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/linear_stream.h"

#include "runtime/memory_manager/graphics_allocation.h"

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
} // namespace NEO
