/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/utilities/stackvec.h"

#include <cstdint>

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class LinearStream;
struct TimestampPacketStorage;
struct RootDeviceEnvironment;

template <typename TagType>
struct TagNode;

struct BlitProperties;
struct HardwareInfo;
struct TimestampPacketDependencies;
using BlitPropertiesContainer = StackVec<BlitProperties, 16>;

struct BlitProperties {
    static BlitProperties constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                CommandStreamReceiver &commandStreamReceiver,
                                                                GraphicsAllocation *memObjAllocation,
                                                                GraphicsAllocation *preallocatedHostAllocation,
                                                                void *hostPtr, uint64_t memObjGpuVa,
                                                                uint64_t hostAllocGpuVa, Vec3<size_t> hostPtrOffset,
                                                                Vec3<size_t> copyOffset, Vec3<size_t> copySize,
                                                                size_t hostRowPitch, size_t hostSlicePitch,
                                                                size_t gpuRowPitch, size_t gpuSlicePitch);

    static BlitProperties constructPropertiesForCopyBuffer(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                           size_t dstOffset, size_t srcOffset, size_t copySize);

    static BlitProperties constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                               GraphicsAllocation *allocation);

    static void setupDependenciesForAuxTranslation(BlitPropertiesContainer &blitPropertiesContainer, TimestampPacketDependencies &timestampPacketDependencies,
                                                   TimestampPacketContainer &kernelTimestamps, const CsrDependencies &depsFromEvents,
                                                   CommandStreamReceiver &gpguCsr, CommandStreamReceiver &bcsCsr);

    TagNode<TimestampPacketStorage> *outputTimestampPacket = nullptr;
    BlitterConstants::BlitDirection blitDirection;
    CsrDependencies csrDependencies;
    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::None;

    GraphicsAllocation *dstAllocation = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    uint64_t dstGpuAddress = 0;
    uint64_t srcGpuAddress = 0;

    Vec3<size_t> copySize = 0;
    Vec3<size_t> dstOffset = 0;
    Vec3<size_t> srcOffset = 0;

    size_t dstRowPitch = 0;
    size_t dstSlicePitch = 0;
    size_t srcRowPitch = 0;
    size_t srcSlicePitch = 0;
};

template <typename GfxFamily>
struct BlitCommandsHelper {
    static size_t estimateBlitCommandsSize(Vec3<size_t> copySize, const CsrDependencies &csrDependencies, bool updateTimestampPacket);
    static size_t estimateBlitCommandsSize(const BlitPropertiesContainer &blitPropertiesContainer, const HardwareInfo &hwInfo);
    static uint64_t calculateBlitCommandDestinationBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice);
    static uint64_t calculateBlitCommandSourceBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice);
    static void dispatchBlitCommandsForBuffer(const BlitProperties &blitProperties, LinearStream &linearStream, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void appendBlitCommandsForBuffer(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);
};
} // namespace NEO
