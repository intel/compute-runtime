/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_id_gen.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_id_gen_special.inl"
#include "shared/source/utilities/cpu_info.h"

#include <array>

namespace NEO {

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
void (*LocalIDHelper::generateSimd8)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize) = generateLocalIDsSimd<uint16x8_t, 8>;
void (*LocalIDHelper::generateSimd16)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize) = generateLocalIDsSimd<uint16x8_t, 16>;
void (*LocalIDHelper::generateSimd32)(void *buffer, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize) = generateLocalIDsSimd<uint16x8_t, 32>;

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

// traditional function to generate local IDs
void generateLocalIDs(void *buffer, uint16_t simd, const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, bool isImageOnlyKernel, uint32_t grfSize, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
    auto threadsPerWorkGroup = static_cast<uint16_t>(gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, static_cast<uint32_t>(localWorkgroupSize[0] * localWorkgroupSize[1] * localWorkgroupSize[2]), grfCount, rootDeviceEnvironment));
    bool useLayoutForImages = isImageOnlyKernel && isCompatibleWithLayoutForImages(localWorkgroupSize, dimensionsOrder, simd);
    if (useLayoutForImages) {
        generateLocalIDsWithLayoutForImages(buffer, localWorkgroupSize, simd);
    } else if (simd == 32) {
        LocalIDHelper::generateSimd32(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder, grfSize != 32);
    } else if (simd == 16) {
        LocalIDHelper::generateSimd16(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder, grfSize != 32);
    } else if (simd == 8) {
        LocalIDHelper::generateSimd8(buffer, localWorkgroupSize, threadsPerWorkGroup, dimensionsOrder, grfSize != 32);
    } else {
        generateLocalIDsForSimdOne(buffer, localWorkgroupSize, dimensionsOrder, grfSize);
    }
}

} // namespace NEO
