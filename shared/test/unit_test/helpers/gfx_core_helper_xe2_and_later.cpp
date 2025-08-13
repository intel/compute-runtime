/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using GfxCoreHelperXe2AndLaterTests = ::testing::Test;
HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenAtLeastXe2HpgWhenCallIsTimestampShiftRequiredThenFalseIsReturned, IsAtLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};

    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isTimestampShiftRequired());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned, IsLNL) {
    DebugManagerStateRestore restorer;

    auto pInHwInfo = *defaultHwInfo;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned, IsBMG) {
    DebugManagerStateRestore restorer;

    auto pInHwInfo = *defaultHwInfo;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenAtLeastXe2HpgWhenEncodeAdditionalTimestampOffsetsThenOffsetsEncoded, IsAtLeastXe2HpgCore) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    constexpr static auto bufferSize = sizeof(MI_STORE_REGISTER_MEM) * 2;

    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    uint64_t fstAddress = 12;
    uint64_t sndAddress = 100;
    MemorySynchronizationCommands<FamilyType>::encodeAdditionalTimestampOffsets(stream, fstAddress, sndAddress, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList storeRegMemList = hwParser.getCommandsList<MI_STORE_REGISTER_MEM>();
    EXPECT_EQ(2u, storeRegMemList.size());
    auto storeRegMemIt = find<MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(storeRegMemIt, hwParser.cmdList.end());

    auto storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*storeRegMemIt);
    EXPECT_EQ(storeRegMem->getRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetHigh);
    EXPECT_EQ(storeRegMem->getMemoryAddress(), fstAddress + sizeof(uint32_t));

    storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*(++storeRegMemIt));
    EXPECT_EQ(storeRegMem->getRegisterAddress(), RegisterOffsets::globalTimestampUn);
    EXPECT_EQ(storeRegMem->getMemoryAddress(), sndAddress + sizeof(uint32_t));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenAtLeastXe2HpgWhenIsCacheFlushPriorImageReadRequiredThenTrueIsReturned, IsAtLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isCacheFlushPriorImageReadRequired());
}
