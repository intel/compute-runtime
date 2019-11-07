/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/kernel_helpers.h"

#include "core/helpers/basic_math.h"

#include <algorithm>

namespace NEO {

uint32_t KernelHelper::getMaxWorkGroupCount(uint32_t simd, uint32_t availableThreadCount, uint32_t dssCount, uint32_t availableSlmSize,
                                            uint32_t usedSlmSize, uint32_t maxBarrierCount, uint32_t numberOfBarriers, uint32_t workDim,
                                            const size_t *localWorkSize) {
    size_t workGroupSize = 1;
    for (uint32_t i = 0; i < workDim; i++) {
        workGroupSize *= localWorkSize[i];
    }

    auto threadsPerThreadGroup = static_cast<uint32_t>(Math::divideAndRoundUp(workGroupSize, simd));
    auto maxWorkGroupsCount = availableThreadCount / threadsPerThreadGroup;

    if (numberOfBarriers > 0) {
        auto maxWorkGroupsCountDueToBarrierUsage = dssCount * (maxBarrierCount / numberOfBarriers);
        maxWorkGroupsCount = std::min(maxWorkGroupsCount, maxWorkGroupsCountDueToBarrierUsage);
    }

    if (usedSlmSize > 0) {
        auto maxWorkGroupsCountDueToSlm = availableSlmSize / usedSlmSize;
        maxWorkGroupsCount = std::min(maxWorkGroupsCount, maxWorkGroupsCountDueToSlm);
    }

    return maxWorkGroupsCount;
}

} // namespace NEO
