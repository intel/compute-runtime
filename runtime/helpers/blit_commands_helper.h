/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_constants.h"
#include "core/utilities/stackvec.h"
#include "runtime/helpers/csr_deps.h"
#include "runtime/helpers/properties_helper.h"

#include <cstdint>

namespace NEO {
struct BuiltinOpParams;
class Context;
class CommandStreamReceiver;
class GraphicsAllocation;
class LinearStream;
struct TimestampPacketStorage;

template <typename TagType>
struct TagNode;

struct BlitProperties {
    static BlitProperties constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                CommandStreamReceiver &commandStreamReceiver,
                                                                GraphicsAllocation *memObjAllocation, size_t memObjOFfset,
                                                                GraphicsAllocation *mapAllocation,
                                                                void *hostPtr, size_t hostPtrOffset,
                                                                size_t copyOffset, uint64_t copySize);

    static BlitProperties constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                CommandStreamReceiver &commandStreamReceiver,
                                                                const BuiltinOpParams &builtinOpParams);

    static BlitProperties constructPropertiesForCopyBuffer(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                           size_t dstOffset, size_t srcOffset, uint64_t copySize);

    static BlitProperties constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                               GraphicsAllocation *allocation);

    static BlitterConstants::BlitDirection obtainBlitDirection(uint32_t commandType);

    TagNode<TimestampPacketStorage> *outputTimestampPacket = nullptr;
    BlitterConstants::BlitDirection blitDirection;
    CsrDependencies csrDependencies;
    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::None;

    GraphicsAllocation *dstAllocation = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    size_t dstOffset = 0;
    size_t srcOffset = 0;
    uint64_t copySize = 0;
};

using BlitPropertiesContainer = StackVec<BlitProperties, 32>;

template <typename GfxFamily>
struct BlitCommandsHelper {
    static size_t estimateBlitCommandsSize(uint64_t copySize, const CsrDependencies &csrDependencies, bool updateTimestampPacket);
    static size_t estimateBlitCommandsSize(const BlitPropertiesContainer &blitPropertiesContainer);
    static void dispatchBlitCommandsForBuffer(const BlitProperties &blitProperties, LinearStream &linearStream);
    static void appendBlitCommandsForBuffer(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd);
};
} // namespace NEO
