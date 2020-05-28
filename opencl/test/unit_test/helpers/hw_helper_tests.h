/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "test.h"

using namespace NEO;

using HwHelperTest = Test<ClDeviceFixture>;

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo);
