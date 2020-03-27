/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

struct HwInfoConfigTest : public ::testing::Test,
                          public PlatformFixture {
    void SetUp() override;
    void TearDown() override;

    HardwareInfo pInHwInfo;
    HardwareInfo outHwInfo;

    PLATFORM *testPlatform = nullptr;
    FeatureTable *testSkuTable = nullptr;
    WorkaroundTable *testWaTable = nullptr;
    GT_SYSTEM_INFO *testSysInfo = nullptr;
};
