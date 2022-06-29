/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwHelperTest = Test<DeviceFixture>;

struct ComputeSlmTestInput {
    uint32_t expected;
    uint32_t slmSize;
};

constexpr ComputeSlmTestInput computeSlmValuesXeHPAndLaterTestsInput[] = {
    {0, 0 * KB},
    {1, 0 * KB + 1},
    {1, 1 * KB},
    {2, 1 * KB + 1},
    {2, 2 * KB},
    {3, 2 * KB + 1},
    {3, 4 * KB},
    {4, 4 * KB + 1},
    {4, 8 * KB},
    {5, 8 * KB + 1},
    {5, 16 * KB},
    {6, 16 * KB + 1},
    {6, 32 * KB},
    {7, 32 * KB + 1},
    {7, 64 * KB}};
