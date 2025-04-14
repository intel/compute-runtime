/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_lnl.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using LnlDeviceIdTest = Test<DeviceFixture>;

LNLTEST_F(LnlDeviceIdTest, givenLnlProductWhenCheckingCapabilitiesThenReturnCorrectValues) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);

    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);

    EXPECT_TRUE(LNL::hwInfo.capabilityTable.supportsImages);

    EXPECT_EQ(512u, pDevice->getHardwareInfo().gtSystemInfo.ThreadCount);
    EXPECT_EQ(1024u, pDevice->getDeviceInfo().maxWorkGroupSize);

    EXPECT_EQ(128u, pDevice->getHardwareInfo().capabilityTable.maxProgrammableSlmSize);

    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits);
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.timestampValidBits);
}
