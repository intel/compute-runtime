/*
 * Copyright (C) 2018-2026 Intel Corporation
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

// Initialize the lookup table based on CPU capabilities
LocalIDHelper::LocalIDHelper() {
}

LocalIDHelper LocalIDHelper::initializer;

// traditional function to generate local IDs
void generateLocalIDs(void *buffer, uint16_t simd, const std::array<uint16_t, 3> &localWorkgroupSize, const std::array<uint8_t, 3> &dimensionsOrder, bool isImageOnlyKernel, uint32_t grfSize, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment, uint8_t activeChannels) {
    bool useLayoutForImages = isImageOnlyKernel && isCompatibleWithLayoutForImages(localWorkgroupSize, dimensionsOrder, simd);
    if (useLayoutForImages) {
        generateLocalIDsWithLayoutForImages(buffer, localWorkgroupSize, simd);
    } else {
        generateLocalIDsForSimdOne(buffer, localWorkgroupSize, dimensionsOrder, grfSize);
    }
}

} // namespace NEO
