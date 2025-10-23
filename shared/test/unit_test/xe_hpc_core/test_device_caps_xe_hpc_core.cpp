/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using XeHpcCoreDeviceCaps = Test<DeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingImageSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingMediaBlockSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenXeHpcCoreWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
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

XE_HPC_CORETEST_F(XeHpcCoreDeviceCaps, givenSlmSizeWhenEncodingThenReturnCorrectValues) {
    struct ComputeSlmTestInput {
        uint32_t expected;
        uint32_t slmSize;
    };

    ComputeSlmTestInput computeSlmValuesXeHpcTestsInput[] = {
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

    auto hwInfo = *defaultHwInfo;

    for (auto &testInput : computeSlmValuesXeHpcTestsInput) {
        EXPECT_EQ(testInput.expected, EncodeDispatchKernel<FamilyType>::computeSlmValues(hwInfo, testInput.slmSize, nullptr, false));
    }

    EXPECT_THROW(EncodeDispatchKernel<FamilyType>::computeSlmValues(hwInfo, 129 * MemoryConstants::kiloByte, nullptr, false), std::exception);
}