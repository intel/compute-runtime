/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class Device;
struct RootDeviceEnvironment;

struct KernelHelper {
    enum class ErrorCode {
        success = 0,
        outOfDeviceMemory = 1,
        invalidKernel = 2
    };
    static uint32_t getMaxWorkGroupCount(const RootDeviceEnvironment &rootDeviceEnvironment, const KernelDescriptor &kernelDescriptor,
                                         uint32_t numSubDevices, uint32_t usedSlmSize, uint32_t workDim, const size_t *localWorkSize, EngineGroupType engineGroupType, bool isEngineInstanced);
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

    static bool isAnyArgumentPtrByValue(const KernelDescriptor &kernelDescriptor);
};

} // namespace NEO
