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

#include "runtime/command_queue/local_id_gen.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/utilities/cpu_info.h"

#include <array>

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
void (*LocalIDHelper::generateSimd8)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder) = generateLocalIDsSimd<uint16x8_t, 8>;
void (*LocalIDHelper::generateSimd16)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder) = generateLocalIDsSimd<uint16x8_t, 16>;
void (*LocalIDHelper::generateSimd32)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder) = generateLocalIDsSimd<uint16x8_t, 32>;

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
void generateLocalIDs(void *buffer, uint16_t simd, const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, bool hasKernelOnlyImages) {
    auto threadsPerWorkGroup = static_cast<uint16_t>(getThreadsPerWG(simd, localWorkgroupSize[0] * localWorkgroupSize[1] * localWorkgroupSize[2]));
    bool use4x4Layout = hasKernelOnlyImages && isCompatibleWith4x4Layout(localWorkgroupSize, dimensionsOrder, simd);
    if (use4x4Layout) {
        generateLocalIDsWith4x4Layout(buffer, localWorkgroupSize, simd);
    } else if (simd == 32) {
        LocalIDHelper::generateSimd32(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder);
    } else if (simd == 16) {
        LocalIDHelper::generateSimd16(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder);
    } else {
        LocalIDHelper::generateSimd8(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder);
    }
}

bool isCompatibleWith4x4Layout(const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, uint16_t simd) {
    //limit support to 8x4x1 and 8x8x1 LWS
    return dimensionsOrder.at(0) == 0 &&
           dimensionsOrder.at(1) == 1 &&
           localWorkgroupSize[2] == 1 &&
           localWorkgroupSize[0] == 8 &&
           (localWorkgroupSize[1] == 4 || localWorkgroupSize[1] == 8);
}

inline void generateLocalIDsWith4x4Layout(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t simd) {
    uint8_t rowWidth = simd == 32u ? 32u : 16u;
    uint8_t xDelta = simd == 8u ? 2u : 4u;                                                  // difference between corresponding values in consecutive X rows
    uint8_t yDelta = simd == 8u || localWorkgroupSize.at(1) == 4u ? 4u : rowWidth / xDelta; // difference between corresponding values in consecutive Y rows
    std::array<uint16_t, 3> replicationFactors{{static_cast<uint16_t>(localWorkgroupSize.at(0) / xDelta),
                                                static_cast<uint16_t>(localWorkgroupSize.at(1) / yDelta),
                                                static_cast<uint16_t>(localWorkgroupSize.at(2))}};
    bool earlyGrowX = replicationFactors.at(1) == 1 && simd == 32u && replicationFactors.at(0) > 1;
    bool earlyGrowZ = replicationFactors.at(1) == 1 && simd == 32u && !earlyGrowX && replicationFactors.at(2) > 1;
    auto buffer = reinterpret_cast<uint16_t *>(b);
    uint16_t offset = 0u;
    for (uint16_t z = 0u; z < replicationFactors.at(2); z++) {
        for (uint16_t y = 0u; y < replicationFactors.at(1); y++) {
            for (uint16_t x = 0u; x < replicationFactors.at(0); x++) {
                // row for X
                for (uint8_t i = 0u; i < simd; i++) {
                    if (earlyGrowX && i == yDelta * xDelta) {
                        x++;
                    }
                    auto xValue = xDelta * x + (i & (xDelta - 1));
                    buffer[offset + i] = xValue & (localWorkgroupSize.at(0) - 1);
                }
                offset += rowWidth;
                // row for Y
                for (uint8_t i = 0u; i < simd; i++) {
                    auto yValue = yDelta * y + i / xDelta;
                    buffer[offset + i] = yValue & (localWorkgroupSize.at(1) - 1);
                }
                offset += rowWidth;
                // row for Z
                for (uint8_t i = 0u; i < simd; i++) {
                    if (earlyGrowZ && i == yDelta * xDelta) {
                        z++;
                    }
                    auto zValue = z;
                    buffer[offset + i] = zValue & (localWorkgroupSize.at(2) - 1);
                }
                offset += rowWidth;
            }
        }
    }
}
} // namespace OCLRT
