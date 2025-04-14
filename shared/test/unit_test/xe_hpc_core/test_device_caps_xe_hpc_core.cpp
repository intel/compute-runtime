/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using XeHpcCoreDeviceCaps = Test<DeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingImageSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingMediaBlockSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingFloatAtomicsSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsFloatAtomics);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingCxlTypeThenReturnZero) {
    EXPECT_EQ(0u, pDevice->getHardwareInfo().capabilityTable.cxlType);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenHwInfoWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(128u, pDevice->getHardwareInfo().capabilityTable.maxProgrammableSlmSize);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    const auto deviceSubgroups = gfxCoreHelper.getDeviceSubGroupSizes();

    EXPECT_EQ(2u, deviceSubgroups.size());
    EXPECT_EQ(16u, deviceSubgroups[0]);
    EXPECT_EQ(32u, deviceSubgroups[1]);
}
