/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/kernel_descriptor.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class Device;

struct KernelHelper {
    enum class ErrorCode {
        SUCCESS = 0,
        OUT_OF_DEVICE_MEMORY = 1,
        INVALID_KERNEL = 2
    };
    static uint32_t getMaxWorkGroupCount(uint32_t simd, uint32_t availableThreadCount, uint32_t dssCount, uint32_t availableSlmSize,
                                         uint32_t usedSlmSize, uint32_t maxBarrierCount, uint32_t numberOfBarriers, uint32_t workDim,
                                         const size_t *localWorkSize);
    static inline uint64_t getPrivateSurfaceSize(uint64_t perHwThreadPrivateMemorySize, uint32_t computeUnitsUsedForScratch) {
        return perHwThreadPrivateMemorySize * computeUnitsUsedForScratch;
    }
    static inline uint64_t getScratchSize(uint64_t perHwThreadScratchSize, uint32_t computeUnitsUsedForScratch) {
        return perHwThreadScratchSize * computeUnitsUsedForScratch;
    }
    static inline uint64_t getPrivateScratchSize(uint64_t perHwThreadPrivateScratchSize, uint32_t computeUnitsUsedForScratch) {
        return perHwThreadPrivateScratchSize * computeUnitsUsedForScratch;
    }
    static ErrorCode checkIfThereIsSpaceForScratchOrPrivate(KernelDescriptor::KernelAttributes attributes, Device *device);
};

} // namespace NEO
