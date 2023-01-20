/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using DeviceSklTest = Test<DeviceFixture>;

SKLTEST_F(DeviceSklTest, givenSklDeviceWhenAskedForProflingTimerResolutionThen83IsReturned) {
    auto resolution = pDevice->getProfilingTimerResolution();
    EXPECT_DOUBLE_EQ(83.333, resolution);
}
