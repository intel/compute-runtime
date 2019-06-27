/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/csr_deps.h"
#include "runtime/memory_manager/memory_constants.h"

#include <cstdint>

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class LinearStream;
class TimestampPacketContainer;

struct BlitProperties {
    BlitProperties() = delete;

    static BlitProperties constructPropertiesForReadWriteBuffer(BlitterConstants::BlitWithHostPtrDirection copyDirection,
                                                                CommandStreamReceiver &commandStreamReceiver,
                                                                GraphicsAllocation *memObjAllocation, void *hostPtr, bool blocking,
                                                                size_t offset, uint64_t copySize);

    TimestampPacketContainer *outputTimestampPacket = nullptr;
    BlitterConstants::BlitWithHostPtrDirection copyDirection;
    CsrDependencies csrDependencies;

    GraphicsAllocation *dstAllocation = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    void *hostPtr = nullptr;
    bool blocking = false;
    size_t dstOffset = 0;
    size_t srcOffset = 0;
    uint64_t copySize = 0;
};

template <typename GfxFamily>
struct BlitCommandsHelper {
    static size_t estimateBlitCommandsSize(uint64_t copySize, const CsrDependencies &csrDependencies, bool updateTimestampPacket);
    static void dispatchBlitCommandsForBuffer(const BlitProperties &blitProperites, LinearStream &linearStream);
    static void appendBlitCommandsForBuffer(const BlitProperties &blitProperites, typename GfxFamily::XY_COPY_BLT &blitCmd);
};
} // namespace NEO
