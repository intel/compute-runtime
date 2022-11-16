/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
class HwInfoConfig;
class ExecutionEnvironment;
} // namespace NEO

using namespace NEO;

struct HwInfoConfigTest : public ::testing::Test {
    HwInfoConfigTest();
    ~HwInfoConfigTest() override;
    void SetUp() override;

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    HardwareInfo pInHwInfo{};
    HardwareInfo outHwInfo{};
    HwInfoConfig *productHelper = nullptr;
    PLATFORM *testPlatform = nullptr;
};
