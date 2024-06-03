/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <optional>

namespace NEO {

constexpr uint64_t probedSizeRegionZero = 8 * MemoryConstants::gigaByte;
constexpr uint64_t probedSizeRegionOne = 16 * MemoryConstants::gigaByte;
constexpr uint64_t probedSizeRegionFour = 32 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionZero = 6 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionOne = 12 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionFour = 4 * MemoryConstants::gigaByte;

class MockIoctlHelper : public IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    ADDMETHOD_CONST_NOBASE(isImmediateVmBindRequired, bool, false, ());
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override {
        return ioctlRequestValue;
    };

    int getDrmParamValue(DrmParam drmParam) const override {
        if (drmParam == DrmParam::memoryClassSystem || drmParam == DrmParam::memoryClassDevice) {
            return IoctlHelperPrelim20::getDrmParamValue(drmParam);
        }
        return drmParamValue;
    }
    int vmBind(const VmBindParams &vmBindParams) override {
        if (failBind.has_value())
            return *failBind ? -1 : 0;
        else
            return IoctlHelperPrelim20::vmBind(vmBindParams);
    }
    int vmUnbind(const VmBindParams &vmBindParams) override {
        if (failBind.has_value())
            return *failBind ? -1 : 0;
        else
            return IoctlHelperPrelim20::vmUnbind(vmBindParams);
    }
    bool isWaitBeforeBindRequired(bool bind) const override {
        if (waitBeforeBindRequired.has_value())
            return *waitBeforeBindRequired;
        else
            return IoctlHelperPrelim20::isWaitBeforeBindRequired(bind);
    }

    bool allocateInterrupt(uint32_t &handle) override {
        allocateInterruptCalled++;
        return IoctlHelperPrelim20::allocateInterrupt(handle);
    }

    bool releaseInterrupt(uint32_t handle) override {
        releaseInterruptCalled++;
        latestReleaseInterruptHandle = handle;
        return IoctlHelperPrelim20::releaseInterrupt(handle);
    }

    std::unique_ptr<MemoryInfo> createMemoryInfo() override {

        std::vector<MemoryRegion> regionInfo(3);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[0].probedSize = probedSizeRegionZero;
        regionInfo[0].unallocatedSize = unallocatedSizeRegionZero;
        regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
        regionInfo[1].probedSize = probedSizeRegionOne;
        regionInfo[1].unallocatedSize = unallocatedSizeRegionOne;
        regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
        regionInfo[2].probedSize = probedSizeRegionFour;
        regionInfo[2].unallocatedSize = unallocatedSizeRegionFour;

        std::unique_ptr<MemoryInfo> memoryInfo = std::make_unique<MemoryInfo>(regionInfo, drm);
        return memoryInfo;
    }

    unsigned int ioctlRequestValue = 1234u;
    int drmParamValue = 1234;
    std::optional<bool> failBind{};
    std::optional<bool> waitBeforeBindRequired{};
    uint32_t allocateInterruptCalled = 0;
    uint32_t releaseInterruptCalled = 0;
    uint32_t latestReleaseInterruptHandle = InterruptId::notUsed;
};
} // namespace NEO
