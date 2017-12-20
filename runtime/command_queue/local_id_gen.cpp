/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/command_queue/local_id_gen.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/utilities/cpu_info.h"

namespace OCLRT {

struct uint16x8_t;
struct uint16x16_t;

// This is the initial value of SIMD for local ID
// computation.  It correlates to the SIMD lane.
// Must be 32byte aligned for AVX2 usage
ALIGNAS(32)
const uint16_t initialLocalID[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

// Lookup table for generating LocalIDs based on the SIMD of the kernel
void (*LocalIDHelper::generateSimd8)(void *buffer, size_t lwsX, size_t lwsY, size_t threadsPerWorkGroup) = generateLocalIDsSimd<uint16x8_t, 8>;
void (*LocalIDHelper::generateSimd16)(void *buffer, size_t lwsX, size_t lwsY, size_t threadsPerWorkGroup) = generateLocalIDsSimd<uint16x8_t, 16>;
void (*LocalIDHelper::generateSimd32)(void *buffer, size_t lwsX, size_t lwsY, size_t threadsPerWorkGroup) = generateLocalIDsSimd<uint16x8_t, 32>;

// Initialize the lookup table based on CPU capabilities
LocalIDHelper::LocalIDHelper() {
    bool supportsAVX2 = CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureAvX2);
    if (supportsAVX2) {
        LocalIDHelper::generateSimd8 = generateLocalIDsSimd<uint16x8_t, 8>;
        LocalIDHelper::generateSimd16 = generateLocalIDsSimd<uint16x16_t, 16>;
        LocalIDHelper::generateSimd32 = generateLocalIDsSimd<uint16x16_t, 32>;
    }
}

LocalIDHelper LocalIDHelper::initializer;

//traditional function to generate local IDs
void generateLocalIDs(void *buffer, uint32_t simd, size_t lwsX, size_t lwsY, size_t lwsZ) {
    auto threadsPerWorkGroup = getThreadsPerWG(simd, lwsX * lwsY * lwsZ);
    if (simd == 32) {
        LocalIDHelper::generateSimd32(buffer, lwsX, lwsY, threadsPerWorkGroup);
    } else if (simd == 16) {
        LocalIDHelper::generateSimd16(buffer, lwsX, lwsY, threadsPerWorkGroup);
    } else {
        LocalIDHelper::generateSimd8(buffer, lwsX, lwsY, threadsPerWorkGroup);
    }
}
} // namespace OCLRT
