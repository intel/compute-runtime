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
class ProductHelper;
class ExecutionEnvironment;
} // namespace NEO

using namespace NEO;

struct ProductHelperTest : public ::testing::Test {
    ProductHelperTest();
    ~ProductHelperTest() override;
    void SetUp() override;

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    HardwareInfo pInHwInfo{};
    HardwareInfo outHwInfo{};
    ProductHelper *productHelper = nullptr;
    PLATFORM *testPlatform = nullptr;
};
