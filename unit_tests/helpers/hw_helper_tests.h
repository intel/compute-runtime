/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/device/device.h"
#include "runtime/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/hw_info_helper.h"

using namespace OCLRT;

class HwHelperFixture : public DeviceFixture {
  protected:
    void SetUp();
    void TearDown();
    HwInfoHelper hwInfoHelper;
};

using HwHelperTest = Test<HwHelperFixture>;

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo);
