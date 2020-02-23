/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "helpers/hw_helper.h"
#include "helpers/hw_info.h"

namespace NEO {

template <typename GfxFamily>
class MockHwHelperWithFenceAllocation : public HwHelperHw<GfxFamily> {
  public:
    bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const override {
        return true;
    }
};

template <typename GfxFamily>
class MockHwHelperWithLocalMemory : public HwHelperHw<GfxFamily> {
  public:
    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const override {
        return true;
    }
};
} // namespace NEO
