/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/blit_properties_container.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/vec.h"

#include <cstdint>

namespace NEO {
struct TimestampPacketDependencies;
class TagNodeBase;
class TimestampPacketContainer;
class GraphicsAllocation;
class CommandStreamReceiver;

struct BlitProperties {
    static BlitProperties constructPropertiesForReadWrite(BlitterConstants::BlitDirection blitDirection,
                                                          CommandStreamReceiver &commandStreamReceiver,
                                                          GraphicsAllocation *memObjAllocation,
                                                          GraphicsAllocation *preallocatedHostAllocation,
                                                          const void *hostPtr, uint64_t memObjGpuVa,
                                                          uint64_t hostAllocGpuVa, const Vec3<size_t> &hostPtrOffset,
                                                          const Vec3<size_t> &copyOffset, Vec3<size_t> copySize,
                                                          size_t hostRowPitch, size_t hostSlicePitch,
                                                          size_t gpuRowPitch, size_t gpuSlicePitch);

    static BlitProperties constructPropertiesForCopy(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                     const Vec3<size_t> &dstOffset, const Vec3<size_t> &srcOffset, Vec3<size_t> copySize,
                                                     size_t srcRowPitch, size_t srcSlicePitch,
                                                     size_t dstRowPitch, size_t dstSlicePitch, GraphicsAllocation *clearColorAllocation);

    static BlitProperties constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                               GraphicsAllocation *allocation, GraphicsAllocation *clearColorAllocation);

    static void setupDependenciesForAuxTranslation(BlitPropertiesContainer &blitPropertiesContainer, TimestampPacketDependencies &timestampPacketDependencies,
                                                   TimestampPacketContainer &kernelTimestamps, const CsrDependencies &depsFromEvents,
                                                   CommandStreamReceiver &gpguCsr, CommandStreamReceiver &bcsCsr);

    TagNodeBase *outputTimestampPacket = nullptr;
    TagNodeBase *multiRootDeviceEventSync = nullptr;
    BlitterConstants::BlitDirection blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
    CsrDependencies csrDependencies;
    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::none;

    GraphicsAllocation *dstAllocation = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    GraphicsAllocation *clearColorAllocation = nullptr;
    uint64_t dstGpuAddress = 0;
    uint64_t srcGpuAddress = 0;

    Vec3<size_t> copySize = 0;
    Vec3<size_t> dstOffset = 0;
    Vec3<size_t> srcOffset = 0;
    bool isSystemMemoryPoolUsed = false;

    size_t dstRowPitch = 0;
    size_t dstSlicePitch = 0;
    size_t srcRowPitch = 0;
    size_t srcSlicePitch = 0;
    Vec3<size_t> dstSize = 0;
    Vec3<size_t> srcSize = 0;
    size_t bytesPerPixel = 1;
    GMM_YUV_PLANE_ENUM dstPlane = GMM_YUV_PLANE_ENUM::GMM_NO_PLANE;
    GMM_YUV_PLANE_ENUM srcPlane = GMM_YUV_PLANE_ENUM::GMM_NO_PLANE;

    bool isImageOperation() const;
};

} // namespace NEO