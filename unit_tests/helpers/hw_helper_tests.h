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
#include "unit_tests/helpers/hw_info_helper.h"

using namespace OCLRT;

class HwHelperTest : public testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;
    HwInfoHelper hwInfoHelper;
};

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo);
