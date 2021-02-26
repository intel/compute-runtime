/*
 * Copyright (C) 2016-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ray_tracing_helper.h"

namespace NEO {
const uint32_t RayTracingHelper::memoryBackedFifoSizePerDss = 0;

size_t RayTracingHelper::getTotalMemoryBackedFifoSize(const Device &device) {
    return 0;
}
size_t RayTracingHelper::getMemoryBackedFifoSizeToPatch() {
    return 0;
}
} // namespace NEO
