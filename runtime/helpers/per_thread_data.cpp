/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/per_thread_data.h"

namespace OCLRT {

size_t PerThreadDataHelper::sendPerThreadData(
    LinearStream &indirectHeap,
    uint32_t simd,
    uint32_t numChannels,
    const size_t localWorkSizes[3]) {
    auto offsetPerThreadData = indirectHeap.getUsed();
    if (numChannels) {
        auto localWorkSize = localWorkSizes[0] * localWorkSizes[1] * localWorkSizes[2];
        auto sizePerThreadDataTotal = getPerThreadDataSizeTotal(simd, numChannels, localWorkSize);
        auto pDest = indirectHeap.getSpace(sizePerThreadDataTotal);

        // Generate local IDs
        DEBUG_BREAK_IF(numChannels != 3);
        generateLocalIDs(pDest, simd, localWorkSizes[0], localWorkSizes[1], localWorkSizes[2]);
    }
    return offsetPerThreadData;
}

uint32_t PerThreadDataHelper::getThreadPayloadSize(const iOpenCL::SPatchThreadPayload &threadPayload, uint32_t simd) {
    uint32_t multiplier = static_cast<uint32_t>(getGRFsPerThread(simd));
    uint32_t threadPayloadSize = 0;
    threadPayloadSize = getNumLocalIdChannels(threadPayload) * multiplier * sizeof(GRF);
    threadPayloadSize += (threadPayload.HeaderPresent) ? sizeof(GRF) : 0;
    threadPayloadSize += (threadPayload.LocalIDFlattenedPresent) ? (sizeof(GRF) * multiplier) : 0;
    threadPayloadSize += (threadPayload.UnusedPerThreadConstantPresent) ? (sizeof(GRF)) : 0;
    return threadPayloadSize;
}
} // namespace OCLRT
