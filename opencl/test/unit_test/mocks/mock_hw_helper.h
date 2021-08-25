/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

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

template <typename GfxFamily>
class MockHwHelperWithCompressionFormat : public HwHelperHw<GfxFamily> {
  public:
    uint32_t getFormatForStatelessCompression(const uint32_t format) const override {
        return compressionFormat;
    }
    uint32_t compressionFormat = 0;
};
} // namespace NEO
