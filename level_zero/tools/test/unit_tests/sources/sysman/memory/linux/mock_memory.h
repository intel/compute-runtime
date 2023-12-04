/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp.h"
#include "level_zero/tools/source/sysman/memory/memory_imp.h"

using namespace NEO;
constexpr uint64_t probedSizeRegionZero = 8 * MemoryConstants::gigaByte;
constexpr uint64_t probedSizeRegionOne = 16 * MemoryConstants::gigaByte;
constexpr uint64_t probedSizeRegionTwo = 4 * MemoryConstants::gigaByte;
constexpr uint64_t probedSizeRegionThree = 16 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionZero = 6 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionOne = 12 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionTwo = 25 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionThree = 3 * MemoryConstants::gigaByte;
namespace L0 {
namespace ult {

struct MockMemoryManagerSysman : public MemoryManagerMock {
    MockMemoryManagerSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

struct MockMemoryNeoDrm : public Drm {
    using Drm::ioctlHelper;
    using Drm::memoryInfo;
    const int mockFd = 33;
    MockMemoryNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}
    std::vector<bool> mockQueryMemoryInfoReturnStatus{};
    bool queryMemoryInfo() override {
        if (!mockQueryMemoryInfoReturnStatus.empty()) {
            return mockQueryMemoryInfoReturnStatus.front();
        }

        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[0].probedSize = probedSizeRegionZero;
        regionInfo[0].unallocatedSize = unallocatedSizeRegionZero;
        regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
        regionInfo[1].probedSize = probedSizeRegionOne;
        regionInfo[1].unallocatedSize = unallocatedSizeRegionOne;

        this->memoryInfo.reset(new MemoryInfo(regionInfo, *this));
        return true;
    }
};

} // namespace ult
} // namespace L0
