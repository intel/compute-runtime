/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/blit_properties_container.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/vec.h"

#include "blit_properties_ext.h"

#include <cstdint>

namespace NEO {
struct TimestampPacketDependencies;
struct BlitSyncPropertiesExt;
class TagNodeBase;
class TimestampPacketContainer;
class GraphicsAllocation;
class CommandStreamReceiver;
class GmmResourceInfo;

enum class BlitSyncMode {
    none = 0,
    timestamp,
    immediate,
    timestampAndImmediate
};

struct BlitSyncProperties {
    EncodePostSyncArgs postSyncArgs{};
    TagNodeBase *outputTimestampPacket = nullptr;
    BlitSyncMode syncMode = BlitSyncMode::none;
    uint64_t deviceGpuWriteAddress = 0;
    uint64_t hostGpuWriteAddress = 0;
    uint64_t timestampGpuWriteAddress = 0;
    uint64_t writeValue = 0;
    bool isTimestampMode() const {
        return (syncMode == BlitSyncMode::timestamp) || (syncMode == BlitSyncMode::timestampAndImmediate);
    }
};

struct BlitProperties {

    static BlitProperties constructPropertiesForMemoryFill(
        GraphicsAllocation *dstAllocation, uint64_t dstPtr,
        size_t size, uint32_t *pattern, size_t patternSize, size_t offset);

    static BlitProperties constructPropertiesForReadWrite(BlitterConstants::BlitDirection blitDirection,
                                                          CommandStreamReceiver &commandStreamReceiver,
                                                          GraphicsAllocation *memObjAllocation,
                                                          GraphicsAllocation *preallocatedHostAllocation,
                                                          const void *hostPtr, uint64_t memObjGpuVa,
                                                          uint64_t hostAllocGpuVa, const Vec3<size_t> &hostPtrOffset,
                                                          const Vec3<size_t> &copyOffset, Vec3<size_t> copySize,
                                                          size_t hostRowPitch, size_t hostSlicePitch,
                                                          size_t gpuRowPitch, size_t gpuSlicePitch);

    static BlitProperties constructPropertiesForCopy(
        GraphicsAllocation *dstAllocation, uint64_t dstPtr,
        GraphicsAllocation *srcAllocation, uint64_t srcPtr,
        const Vec3<size_t> &dstOffset, const Vec3<size_t> &srcOffset, Vec3<size_t> copySize,
        size_t srcRowPitch, size_t srcSlicePitch,
        size_t dstRowPitch, size_t dstSlicePitch,
        GraphicsAllocation *clearColorAllocation);

    static BlitProperties constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                               GraphicsAllocation *allocation, GraphicsAllocation *clearColorAllocation);

    static void setupDependenciesForAuxTranslation(BlitPropertiesContainer &blitPropertiesContainer, TimestampPacketDependencies &timestampPacketDependencies,
                                                   TimestampPacketContainer &kernelTimestamps, const CsrDependencies &depsFromEvents,
                                                   CommandStreamReceiver &gpguCsr, CommandStreamReceiver &bcsCsr);

    bool isImageOperation() const;
    bool isSrc1DTiledArray() const;
    bool isDst1DTiledArray() const;
    bool is1DTiledArray(GmmResourceInfo *resInfo) const;
    void transform1DArrayTo2DArrayIfNeeded();

    BlitSyncProperties blitSyncProperties = {};
    CsrDependencies csrDependencies;
    TagNodeBase *multiRootDeviceEventSync = nullptr;

    BlitterConstants::BlitDirection blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::none;

    GraphicsAllocation *dstAllocation = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    GraphicsAllocation *clearColorAllocation = nullptr;
    void *lastBlitCommand = nullptr;
    uint32_t *fillPattern = nullptr;
    uint64_t dstGpuAddress = 0;
    uint64_t srcGpuAddress = 0;
    uint32_t computeStreamPartitionCount = 0;

    Vec3<size_t> copySize = 0;
    Vec3<size_t> dstOffset = 0;
    Vec3<size_t> srcOffset = 0;

    size_t dstRowPitch = 0;
    size_t dstSlicePitch = 0;
    size_t srcRowPitch = 0;
    size_t srcSlicePitch = 0;
    size_t fillPatternSize = 0;
    Vec3<size_t> dstSize = 0;
    Vec3<size_t> srcSize = 0;
    size_t bytesPerPixel = 1;
    GMM_YUV_PLANE_ENUM dstPlane = GMM_YUV_PLANE_ENUM::GMM_NO_PLANE;
    GMM_YUV_PLANE_ENUM srcPlane = GMM_YUV_PLANE_ENUM::GMM_NO_PLANE;
    bool isSystemMemoryPoolUsed = false;
    bool highPriority = false;

    BlitPropertiesExt propertiesExt{};
};

} // namespace NEO
