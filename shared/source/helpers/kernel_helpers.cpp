/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kernel_helpers.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include <algorithm>

namespace NEO {

uint32_t KernelHelper::getMaxWorkGroupCount(uint32_t simd, uint32_t availableThreadCount, uint32_t dssCount, uint32_t availableSlmSize,
                                            uint32_t usedSlmSize, uint32_t maxBarrierCount, uint32_t numberOfBarriers, uint32_t workDim,
                                            const size_t *localWorkSize) {
    UNRECOVERABLE_IF((workDim == 0) || (workDim > 3));
    UNRECOVERABLE_IF(localWorkSize == nullptr);

    size_t workGroupSize = localWorkSize[0];
    for (uint32_t i = 1; i < workDim; i++) {
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
