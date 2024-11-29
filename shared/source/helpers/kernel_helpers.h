/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace NEO {
class Device;
class GraphicsAllocation;
struct RootDeviceEnvironment;

struct KernelHelper {
    enum class ErrorCode {
        success = 0,
        outOfDeviceMemory = 1,
        invalidKernel = 2
    };
    static uint32_t getMaxWorkGroupCount(Device &device, uint16_t numGrfRequired, uint8_t simdSize, uint8_t barrierCount, uint32_t alignedSlmSize, uint32_t workDim, const size_t *localWorkSize,
                                         EngineGroupType engineGroupType, bool implicitScalingEnabled, bool forceSingleTileQuery);
    static uint32_t getMaxWorkGroupCount(const RootDeviceEnvironment &rootDeviceEnvironment, uint16_t numGrfRequired, uint8_t simdSize, uint8_t barrierCount,
                                         uint32_t numSubDevices, uint32_t usedSlmSize, uint32_t workDim, const size_t *localWorkSize, EngineGroupType engineGroupType);
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

    static inline size_t getRegionGroupBarrierSize(const size_t threadGroupCount, const size_t localRegionSize) {
        return alignUp((threadGroupCount / localRegionSize) * (localRegionSize + 1) * 2 * sizeof(uint32_t), MemoryConstants::cacheLineSize);
    }

    static std::pair<GraphicsAllocation *, size_t> getRegionGroupBarrierAllocationOffset(Device &device, const size_t threadGroupCount, const size_t localRegionSize);

    static inline size_t getSyncBufferSize(const size_t requestedNumberOfWorkgroups) {
        return alignUp(std::max(requestedNumberOfWorkgroups, static_cast<size_t>(CommonConstants::minimalSyncBufferSize)), static_cast<size_t>(CommonConstants::maximalSizeOfAtomicType));
    }
    static std::pair<GraphicsAllocation *, size_t> getSyncBufferAllocationOffset(Device &device, const size_t requestedNumberOfWorkgroups);
};

} // namespace NEO
