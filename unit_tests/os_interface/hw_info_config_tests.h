/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/device/device.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;
using namespace std;

struct HwInfoConfigTest : public ::testing::Test,
                          public PlatformFixture {
    void SetUp() override;
    void TearDown() override;
    void ReleaseOutHwInfoStructs();

    HardwareInfo *pInHwInfo;
    HardwareInfo outHwInfo;

    RuntimeCapabilityTable originalCapTable;

    const PLATFORM *pOldPlatform;
    PLATFORM testPlatform;

    const FeatureTable *pOldSkuTable;
    FeatureTable testSkuTable;

    const WorkaroundTable *pOldWaTable;
    WorkaroundTable testWaTable;

    const GT_SYSTEM_INFO *pOldSysInfo;
    GT_SYSTEM_INFO testSysInfo;
};
