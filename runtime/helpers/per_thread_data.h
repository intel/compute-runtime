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

#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include "runtime/command_queue/local_id_gen.h"
#include "patch_shared.h"

namespace OCLRT {
class LinearStream;

struct PerThreadDataHelper {
    static inline size_t getLocalIdSizePerThread(
        uint32_t simd,
        uint32_t numChannels) {
        return getPerThreadSizeLocalIDs(simd, numChannels);
    }

    static inline size_t getPerThreadDataSizeTotal(
        uint32_t simd,
        uint32_t numChannels,
        size_t localWorkSize) {
        return getThreadsPerWG(simd, localWorkSize) * getLocalIdSizePerThread(simd, numChannels);
    }

    static size_t sendPerThreadData(
        LinearStream &indirectHeap,
        uint32_t simd,
        uint32_t numChannels,
        const size_t localWorkSizes[3],
        const std::array<uint8_t, 3> &workgroupWalkOrder,
        bool hasKernelOnlyImages);

    static inline uint32_t getNumLocalIdChannels(const iOpenCL::SPatchThreadPayload &threadPayload) {
        return threadPayload.LocalIDXPresent +
               threadPayload.LocalIDYPresent +
               threadPayload.LocalIDZPresent;
    }

    static uint32_t getThreadPayloadSize(const iOpenCL::SPatchThreadPayload &threadPayload, uint32_t simd);
};
} // namespace OCLRT
