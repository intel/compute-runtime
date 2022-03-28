/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using MultiTileImmediateCommandListTest = Test<MultiTileCommandListFixture<true, false, false>>;

HWTEST2_F(MultiTileImmediateCommandListTest, GivenMultiTileDeviceWhenCreatingImmediateCommandListThenExpectPartitionCountMatchTileCount, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, commandList->partitionCount);
}

using MultiTileImmediateInternalCommandListTest = Test<MultiTileCommandListFixture<true, true, false>>;

HWTEST2_F(MultiTileImmediateInternalCommandListTest, GivenMultiTileDeviceWhenCreatingInternalImmediateCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using MultiTileCopyEngineCommandListTest = Test<MultiTileCommandListFixture<false, false, true>>;

HWTEST2_F(MultiTileCopyEngineCommandListTest, GivenMultiTileDeviceWhenCreatingCopyEngineCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using CommandListExecuteImmediate = Test<DeviceFixture>;
HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenRequiredStreamStateIsCorrectlyReported, IsAtLeastSkl) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    auto &currentCsrStreamProperties = commandListImmediate.csr->getStreamProperties();

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::RoundRobin;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);

    int expectedDisableOverdispatch = hwInfoConfig.isDisableOverdispatchAvailable(*defaultHwInfo);
    bool expectedIsCoherencyRequired = hwHelper.forceNonGpuCoherencyWA(true);
    EXPECT_EQ(1, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(expectedDisableOverdispatch, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedIsCoherencyRequired, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(1, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::AgeBased;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);

    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(0, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(0, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(0, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    commandListImmediate.containsAnyKernel = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false);
    EXPECT_FALSE(commandListImmediate.containsAnyKernel);
}

} // namespace ult
} // namespace L0
