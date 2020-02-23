/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> BdwDeviceCaps;

BDWTEST_F(BdwDeviceCaps, kmdNotifyMechanism) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}
