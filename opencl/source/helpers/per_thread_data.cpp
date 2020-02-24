/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/per_thread_data.h"

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/debug_helpers.h"

#include <array>

namespace NEO {

size_t PerThreadDataHelper::sendPerThreadData(
    LinearStream &indirectHeap,
    uint32_t simd,
    uint32_t grfSize,
    uint32_t numChannels,
    const size_t localWorkSizes[3],
    const std::array<uint8_t, 3> &workgroupWalkOrder,
    bool hasKernelOnlyImages) {
    auto offsetPerThreadData = indirectHeap.getUsed();
    if (numChannels) {
        auto localWorkSize = localWorkSizes[0] * localWorkSizes[1] * localWorkSizes[2];
        auto sizePerThreadDataTotal = getPerThreadDataSizeTotal(simd, grfSize, numChannels, localWorkSize);
        auto pDest = indirectHeap.getSpace(sizePerThreadDataTotal);

        // Generate local IDs
        DEBUG_BREAK_IF(numChannels != 3);
        generateLocalIDs(pDest, static_cast<uint16_t>(simd),
                         std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSizes[0]),
                                                  static_cast<uint16_t>(localWorkSizes[1]),
                                                  static_cast<uint16_t>(localWorkSizes[2])}},
                         std::array<uint8_t, 3>{{workgroupWalkOrder[0], workgroupWalkOrder[1], workgroupWalkOrder[2]}},
                         hasKernelOnlyImages, grfSize);
    }
    return offsetPerThreadData;
}

uint32_t PerThreadDataHelper::getThreadPayloadSize(const iOpenCL::SPatchThreadPayload &threadPayload, uint32_t simd, uint32_t grfSize) {
    uint32_t multiplier = static_cast<uint32_t>(getGRFsPerThread(simd, grfSize));
    uint32_t threadPayloadSize = 0;
    threadPayloadSize = getNumLocalIdChannels(threadPayload) * multiplier * grfSize;
    threadPayloadSize += (threadPayload.HeaderPresent) ? grfSize : 0;
    threadPayloadSize += (threadPayload.LocalIDFlattenedPresent) ? (grfSize * multiplier) : 0;
    threadPayloadSize += (threadPayload.UnusedPerThreadConstantPresent) ? grfSize : 0;
    return threadPayloadSize;
}
} // namespace NEO
