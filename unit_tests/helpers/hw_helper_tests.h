/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_helper.h"
#include "runtime/device/device.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

using HwHelperTest = Test<DeviceFixture>;

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo);
