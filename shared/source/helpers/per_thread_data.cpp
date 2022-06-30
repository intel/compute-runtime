/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/per_thread_data.h"

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/debug_helpers.h"

#include <array>

namespace NEO {

size_t PerThreadDataHelper::sendPerThreadData(
    LinearStream &indirectHeap,
    uint32_t simd,
    uint32_t grfSize,
    uint32_t numChannels,
    const std::array<uint16_t, 3> &localWorkSizes,
    const std::array<uint8_t, 3> &workgroupWalkOrder,
    bool hasKernelOnlyImages) {
    auto offsetPerThreadData = indirectHeap.getUsed();
    if (numChannels) {
        size_t localWorkSize = static_cast<size_t>(localWorkSizes[0]) * static_cast<size_t>(localWorkSizes[1]) * static_cast<size_t>(localWorkSizes[2]);
        auto sizePerThreadDataTotal = getPerThreadDataSizeTotal(simd, grfSize, numChannels, localWorkSize);
        auto pDest = indirectHeap.getSpace(sizePerThreadDataTotal);

        // Generate local IDs
        DEBUG_BREAK_IF(numChannels != 3);
        generateLocalIDs(pDest, static_cast<uint16_t>(simd), localWorkSizes, workgroupWalkOrder, hasKernelOnlyImages, grfSize);
    }
    return offsetPerThreadData;
}

uint32_t PerThreadDataHelper::getThreadPayloadSize(const KernelDescriptor &kernelDescriptor, uint32_t grfSize) {
    uint32_t multiplier = static_cast<uint32_t>(getGRFsPerThread(kernelDescriptor.kernelAttributes.simdSize, grfSize));
    uint32_t threadPayloadSize = 0;
    threadPayloadSize = kernelDescriptor.kernelAttributes.numLocalIdChannels * multiplier * grfSize;
    threadPayloadSize += (kernelDescriptor.kernelAttributes.flags.perThreadDataHeaderIsPresent) ? grfSize : 0;
    threadPayloadSize += (kernelDescriptor.kernelAttributes.flags.usesFlattenedLocalIds) ? (grfSize * multiplier) : 0;
    threadPayloadSize += (kernelDescriptor.kernelAttributes.flags.perThreadDataUnusedGrfIsPresent) ? grfSize : 0;
    return threadPayloadSize;
}
} // namespace NEO
