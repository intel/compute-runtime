/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using Xe2HpgCoreDeviceCaps = Test<DeviceFixture>;

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenXe2HpgCoreWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenXe2HpgCoreWhenCheckingImageSupportThenReturnFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenXe2HpgCoreWhenCheckingMediaBlockSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenXe2HpgCoreWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenXe2HpgCoreWhenCheckingFloatAtomicsSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsFloatAtomics);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenXe2HpgCoreWhenCheckingCxlTypeThenReturnZero) {
    EXPECT_EQ(0u, pDevice->getHardwareInfo().capabilityTable.cxlType);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenXe2HpgCoreWhenCheckingDefaultPreemptionModeThenDefaultPreemptionModeIsMidThread) {
    EXPECT_EQ(PreemptionMode::MidThread, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    const auto deviceSubgroups = gfxCoreHelper.getDeviceSubGroupSizes();

    EXPECT_EQ(2u, deviceSubgroups.size());
    EXPECT_EQ(16u, deviceSubgroups[0]);
    EXPECT_EQ(32u, deviceSubgroups[1]);
}

XE2_HPG_CORETEST_F(Xe2HpgCoreDeviceCaps, givenSlmSizeWhenEncodingThenReturnCorrectValues) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    ComputeSlmTestInput computeSlmValuesXe2AndLaterTestsInput[] = {
        {0, 0 * MemoryConstants::kiloByte},
        {1, 0 * MemoryConstants::kiloByte + 1},
        {1, 1 * MemoryConstants::kiloByte},
        {2, 1 * MemoryConstants::kiloByte + 1},
        {2, 2 * MemoryConstants::kiloByte},
        {3, 2 * MemoryConstants::kiloByte + 1},
        {3, 4 * MemoryConstants::kiloByte},
        {4, 4 * MemoryConstants::kiloByte + 1},
        {4, 8 * MemoryConstants::kiloByte},
        {5, 8 * MemoryConstants::kiloByte + 1},
        {5, 16 * MemoryConstants::kiloByte},
        {8, 16 * MemoryConstants::kiloByte + 1},
        {8, 24 * MemoryConstants::kiloByte},
        {6, 24 * MemoryConstants::kiloByte + 1},
        {6, 32 * MemoryConstants::kiloByte},
        {9, 32 * MemoryConstants::kiloByte + 1},
        {9, 48 * MemoryConstants::kiloByte},
        {7, 48 * MemoryConstants::kiloByte + 1},
        {7, 64 * MemoryConstants::kiloByte},
        {10, 64 * MemoryConstants::kiloByte + 1},
        {10, 96 * MemoryConstants::kiloByte},
        {11, 96 * MemoryConstants::kiloByte + 1},
        {11, 128 * MemoryConstants::kiloByte}};

    for (const auto &testInput : computeSlmValuesXe2AndLaterTestsInput) {
        EXPECT_EQ(testInput.expected, gfxCoreHelper.computeSlmValues(hwInfo, testInput.slmSize, nullptr, false));
    }

    EXPECT_THROW(gfxCoreHelper.computeSlmValues(hwInfo, 128 * MemoryConstants::kiloByte + 1, nullptr, false), std::exception);
}
