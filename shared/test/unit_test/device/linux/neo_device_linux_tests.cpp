/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

using DeviceTest = Test<DeviceFixture>;

TEST_F(DeviceTest, GivenDeviceWhenGetAdapterLuidThenLuidIsNotSet) {
    std::array<uint8_t, HwInfoConfig::luidSize> luid, expectedLuid;
    expectedLuid.fill(-1);
    luid = expectedLuid;
    pDevice->getAdapterLuid(luid);

    EXPECT_EQ(expectedLuid, luid);
}

TEST_F(DeviceTest, GivenDeviceWhenVerifyAdapterLuidThenFalseIsReturned) {
    EXPECT_FALSE(pDevice->verifyAdapterLuid());
}

TEST_F(DeviceTest, GivenDeviceWhenGetAdapterMaskThenMaskIsNotSet) {
    uint32_t nodeMask = 0x1234u;
    pDevice->getAdapterMask(nodeMask);

    EXPECT_EQ(nodeMask, 0x1234u);
}

TEST_F(DeviceTest, GivenCallBaseVerifyAdapterLuidWhenGetAdapterMaskThenMaskIsSet) {
    uint32_t nodeMask = 0x1234u;
    pDevice->callBaseVerifyAdapterLuid = false;
    pDevice->getAdapterMask(nodeMask);

    EXPECT_EQ(nodeMask, 1u);
}
