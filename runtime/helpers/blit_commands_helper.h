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
    static size_t estimateBlitCommandsSize(uint64_t copySize, CsrDependencies &csrDependencies);
    static void dispatchBlitCommandsForBuffer(Buffer &dstBuffer, Buffer &srcBuffer, LinearStream &linearStream,
                                              uint64_t dstOffset, uint64_t srcOffset, uint64_t copySize);
    static void appendBlitCommandsForBuffer(Buffer &dstBuffer, Buffer &srcBuffer, typename GfxFamily::XY_COPY_BLT &blitCmd);
};
} // namespace NEO
