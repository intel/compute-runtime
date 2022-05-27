/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

struct HwInfoConfigTest : public ::testing::Test {
    void SetUp() override {
        pInHwInfo = *defaultHwInfo;

        testPlatform = &pInHwInfo.platform;
        testSkuTable = &pInHwInfo.featureTable;
        testWaTable = &pInHwInfo.workaroundTable;
        testSysInfo = &pInHwInfo.gtSystemInfo;

        outHwInfo = {};
    }

    HardwareInfo pInHwInfo;
    HardwareInfo outHwInfo;

    PLATFORM *testPlatform = nullptr;
    FeatureTable *testSkuTable = nullptr;
    WorkaroundTable *testWaTable = nullptr;
    GT_SYSTEM_INFO *testSysInfo = nullptr;
};
