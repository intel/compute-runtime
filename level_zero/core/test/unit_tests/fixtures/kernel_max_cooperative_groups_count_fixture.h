/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

class KernelImpSuggestMaxCooperativeGroupCountFixture : public DeviceFixture {
  public:
    const uint32_t numGrf = 128;
    const uint32_t simd = 8;
    const uint32_t lws[3] = {1, 1, 1};
    uint32_t usedSlm = 0;
    uint32_t usesBarriers = 0;

    uint32_t availableThreadCount;
    uint32_t dssCount;
    uint32_t availableSlm;
    uint32_t maxBarrierCount;
    WhiteBox<::L0::KernelImmutableData> kernelInfo;
    NEO::KernelDescriptor kernelDescriptor;

    void setUp();

    uint32_t getMaxWorkGroupCount();
};
} // namespace ult
} // namespace L0
