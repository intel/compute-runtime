/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe3_core/hw_cmds_xe3_core.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using Xe3CoreDeviceCaps = Test<DeviceFixture>;

XE3_CORETEST_F(Xe3CoreDeviceCaps, givenXe3CoreProductWhenCheckingCapabilitiesThenReturnCorrectValues) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);

    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);

    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);

    EXPECT_EQ(1024u, pDevice->getDeviceInfo().maxWorkGroupSize);

    EXPECT_EQ(128u, pDevice->getHardwareInfo().capabilityTable.maxProgrammableSlmSize);

    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits);
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.timestampValidBits);
}

XE3_CORETEST_F(Xe3CoreDeviceCaps, givenXe3CoreWhenCheckingMediaBlockSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

XE3_CORETEST_F(Xe3CoreDeviceCaps, givenXe3CoreWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
}

XE3_CORETEST_F(Xe3CoreDeviceCaps, givenXe3CoreWhenCheckingDefaultPreemptionModeThenDefaultPreemptionModeIsMidThread) {
    EXPECT_EQ(PreemptionMode::MidThread, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

XE3_CORETEST_F(Xe3CoreDeviceCaps, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    const auto deviceSubgroups = gfxCoreHelper.getDeviceSubGroupSizes();

    EXPECT_EQ(2u, deviceSubgroups.size());
    EXPECT_EQ(16u, deviceSubgroups[0]);
    EXPECT_EQ(32u, deviceSubgroups[1]);
}
