/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstddef>
#include <cstdint>

namespace NEO {

struct KernelHelper {
    static uint32_t getMaxWorkGroupCount(uint32_t simd, uint32_t availableThreadCount, uint32_t dssCount, uint32_t availableSlmSize,
                                         uint32_t usedSlmSize, uint32_t maxBarrierCount, uint32_t numberOfBarriers, uint32_t workDim,
                                         const size_t *localWorkSize);
};

} // namespace NEO
