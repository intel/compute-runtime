/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds_xe_hpc_core_base.h"

using namespace NEO;
using GfxCoreHelperXeHpcCoreTest = ::testing::Test;

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, givenSlmSizeWhenEncodingThenReturnCorrectValues) {
    ComputeSlmTestInput computeSlmValuesXeHpcTestsInput[] = {
        {0, 0 * KB},
        {1, 0 * KB + 1},
        {1, 1 * KB},
        {2, 1 * KB + 1},
        {2, 2 * KB},
        {3, 2 * KB + 1},
        {3, 4 * KB},
        {4, 4 * KB + 1},
        {4, 8 * KB},
        {5, 8 * KB + 1},
        {5, 16 * KB},
        {8, 16 * KB + 1},
        {8, 24 * KB},
        {6, 24 * KB + 1},
        {6, 32 * KB},
        {9, 32 * KB + 1},
        {9, 48 * KB},
        {7, 48 * KB + 1},
        {7, 64 * KB},
        {10, 64 * KB + 1},
        {10, 96 * KB},
        {11, 96 * KB + 1},
        {11, 128 * KB}};

    auto hwInfo = *defaultHwInfo;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    for (auto &testInput : computeSlmValuesXeHpcTestsInput) {
        EXPECT_EQ(testInput.expected, gfxCoreHelper.computeSlmValues(hwInfo, testInput.slmSize));
    }

    EXPECT_THROW(gfxCoreHelper.computeSlmValues(hwInfo, 129 * KB), std::exception);
}

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, WhenGettingIsCpuImageTransferPreferredThenTrueIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isCpuImageTransferPreferred(*defaultHwInfo));
}

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, givenGfxCoreHelperWhenGettingIfRevisionSpecificBinaryBuiltinIsRequiredThenTrueIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isRevisionSpecificBinaryBuiltinRequired());
}

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, givenGfxCoreHelperWhenCheckTimestampWaitSupportThenReturnTrue) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isTimestampWaitSupportedForQueues());
}

using ProductHelperTestXeHpcCore = Test<DeviceFixture>;

XE_HPC_CORETEST_F(ProductHelperTestXeHpcCore, givenProductHelperWhenCheckTimestampWaitSupportForEventsThenReturnTrue) {
    auto &helper = getHelper<ProductHelper>();
    EXPECT_TRUE(helper.isTimestampWaitSupportedForEvents());
}

XE_HPC_CORETEST_F(GfxCoreHelperXeHpcCoreTest, givenXeHPCPlatformWhenCheckAssignEngineRoundRobinSupportedThenReturnTrue) {
    auto hwInfo = *defaultHwInfo;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.isAssignEngineRoundRobinSupported(hwInfo), ProductHelper::get(hwInfo.platform.eProductFamily)->isAssignEngineRoundRobinSupported());
}

XE_HPC_CORETEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCallCopyThroughLockedPtrEnabledThenReturnTrue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo));
}

XE_HPC_CORETEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCallGetAmountOfAllocationsToFillThenReturnTrue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, givenGfxCoreHelperWhenAskingForRelaxedOrderingSupportThenReturnFalse, IGFX_XE_HPC_CORE);

XE_HPC_CORETEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenAskingForRelaxedOrderingSupportThenReturnTrue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_TRUE(gfxCoreHelper.isRelaxedOrderingSupported());
}