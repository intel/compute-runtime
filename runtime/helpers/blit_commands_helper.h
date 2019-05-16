/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
class Buffer;
class GraphicsAllocation;
class LinearStream;

template <typename GfxFamily>
struct BlitCommandsHelper {
    static size_t estimateBlitCommandsSize(uint64_t copySize);
    static void dispatchBlitCommandsForBuffer(Buffer &dstBuffer, LinearStream &linearStream, GraphicsAllocation &srcAllocation, uint64_t copySize);
    static void appendBlitCommandsForBuffer(Buffer &dstBuffer, GraphicsAllocation &srcAllocation, typename GfxFamily::XY_COPY_BLT &blitCmd);
};
} // namespace NEO
