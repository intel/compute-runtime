/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"

#include <optional>

namespace NEO {

template <typename GfxFamily>
class MockGfxCoreHelperHw : public GfxCoreHelperHw<GfxFamily> {
  public:
    bool isFenceAllocationRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const override {
        return true;
    }
    void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const override {
        allocationData.storageInfo.isLockable = setIsLockable;
    }
    void alignThreadGroupCountToDssSize(uint32_t &threadCount, uint32_t dssCount, uint32_t threadsPerDss, uint32_t threadGroupSize) const override {
        alignThreadGroupCountToDssSizeCalledTimes++;
    }
    bool isPerContextDebugSipRequired() const override {
        return perContextSipRequired;
    }
    bool isSipExternalLibRequired() const override {
        return sipExternalLibRequired;
    }
    uint32_t calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) const override {
        if (this->availableThreadCount.has_value()) {
            return this->availableThreadCount.value();
        }
        return GfxCoreHelperHw<GfxFamily>::calculateAvailableThreadCount(hwInfo, grfCount, rootDeviceEnvironment);
    }
    mutable uint32_t alignThreadGroupCountToDssSizeCalledTimes = 0;
    bool setIsLockable = true;
    bool perContextSipRequired = false;
    bool sipExternalLibRequired = false;
    std::optional<uint32_t> availableThreadCount;
};
} // namespace NEO
