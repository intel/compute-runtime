/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/device/device.h"
#include "runtime/helpers/hw_helper.h"
#include "test.h"

using namespace OCLRT;

class HwHelperTest : public testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;

    PLATFORM testPlatform;
    FeatureTable testFtrTable;
    WorkaroundTable testWaTable;
    GT_SYSTEM_INFO testSysInfo;
    HardwareInfo hwInfo;
};

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo);
