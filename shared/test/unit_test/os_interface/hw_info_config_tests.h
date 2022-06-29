/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct HwInfoConfigTest : public ::testing::Test {
    void SetUp() override {
        pInHwInfo = *defaultHwInfo;
        testPlatform = &pInHwInfo.platform;
    }

    HardwareInfo pInHwInfo;
    HardwareInfo outHwInfo{};

    PLATFORM *testPlatform = nullptr;
};
