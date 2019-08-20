/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

using LkfTest = Test<DeviceFixture>;

LKFTEST_F(LkfTest, givenLkfWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}
