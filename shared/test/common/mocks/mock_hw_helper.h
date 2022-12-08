/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

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
} // namespace NEO
