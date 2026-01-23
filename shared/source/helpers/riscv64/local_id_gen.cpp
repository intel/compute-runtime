/*
 * Copyright (C) 2018-2024 Intel Corporation
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

// Initialize the lookup table based on CPU capabilities
LocalIDHelper::LocalIDHelper() {
}

LocalIDHelper LocalIDHelper::initializer;

void generateLocalIDs(void *buffer, uint16_t simd, const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, bool isImageOnlyKernel, uint32_t grfSize, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) {
    generateLocalIDsForSimdOne(buffer, localWorkgroupSize, dimensionsOrder, grfSize);
}

} // namespace NEO
