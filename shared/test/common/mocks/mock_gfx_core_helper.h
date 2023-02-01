/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"

namespace NEO {

template <typename GfxFamily>
class MockGfxCoreHelperWithFenceAllocation : public GfxCoreHelperHw<GfxFamily> {
  public:
    bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const override {
        return true;
    }
};

template <typename GfxFamily>
class MockGfxCoreHelperWithLocalMemory : public GfxCoreHelperHw<GfxFamily> {
  public:
    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const override {
        return true;
    }
};

template <typename GfxFamily>
struct MockGfxCoreHelperHwWithSetIsLockable : public GfxCoreHelperHw<GfxFamily> {
    void setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const override {
        allocationData.storageInfo.isLockable = setIsLockable;
    }
    bool setIsLockable = true;
};
} // namespace NEO
