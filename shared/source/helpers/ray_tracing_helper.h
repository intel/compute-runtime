/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstdint>
namespace NEO {
class RayTracingHelper : public NonCopyableOrMovableClass {
  public:
    static constexpr uint32_t stackDssMultiplier = 2048;
    static constexpr uint32_t hitInfoSize = 64;
    static constexpr uint32_t bvhStackSize = 96;
    static constexpr uint32_t memoryBackedFifoSizePerDss = 8 * KB;
    static constexpr uint32_t maxBvhLevels = 8;

    static size_t getDispatchGlobalSize(const Device &device, uint32_t maxBvhLevel, uint32_t extraBytesLocal, uint32_t extraBytesGlobal) {
        return static_cast<size_t>(alignUp(getRtGlobalsSize(), MemoryConstants::cacheLineSize) +
                                   getStackSizePerRay(maxBvhLevel, extraBytesLocal) * getNumRtStacks(device) +
                                   extraBytesGlobal);
    }

    static size_t getTotalMemoryBackedFifoSize(const Device &device) {
        return static_cast<size_t>(getNumDss(device)) * memoryBackedFifoSizePerDss;
    }

    static size_t getMemoryBackedFifoSizeToPatch() {
        return static_cast<size_t>(Math::log2(memoryBackedFifoSizePerDss / KB) - 1);
    }

    static size_t getRtGlobalsSize();

    static uint32_t getNumRtStacks(const Device &device) {
        return device.getHardwareInfo().gtSystemInfo.DualSubSliceCount * stackDssMultiplier;
    }

    static uint32_t getNumRtStacksPerDss(const Device &device) {
        return stackDssMultiplier;
    }

    static uint32_t getNumDss(const Device &device) {
        return device.getHardwareInfo().gtSystemInfo.DualSubSliceCount;
    }

    static uint32_t getStackSizePerRay(uint32_t maxBvhLevel, uint32_t extraBytesLocal) {
        return alignUp((hitInfoSize + bvhStackSize * maxBvhLevel +
                        extraBytesLocal),
                       MemoryConstants::cacheLineSize);
    }
};
} // namespace NEO
