/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/xe_hpg_core/cmdlist_xe_hpg_core.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {
using CommandListTests = Test<DeviceFixture>;

HWTEST2_F(CommandListTests, givenDG2WithBSteppingWhenCreatingCommandListThenAdditionalStateBaseAddressCmdIsAdded, IsDG2) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    ze_result_t returnValue;
    auto &hwInfo = *neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &productHelper = neoDevice->getProductHelper();

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());

    itor++;
    itor = find<STATE_BASE_ADDRESS *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
}
HWTEST2_F(CommandListTests, GivenKernelWithDpasWhenUpdateStreamPropertiesForRegularCommandListsCalledAndLwsIsOddThenFusedEuIsDisabled, IsDG2) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    const ze_group_count_t launchKernelArgs = {3, 1, 1};
    kernel.groupSize[0] = 7;
    kernel.groupSize[1] = 1;
    kernel.groupSize[2] = 1;
    commandList->updateStreamPropertiesForRegularCommandLists(kernel, false, launchKernelArgs, false);
    EXPECT_TRUE(commandList->finalStreamState.frontEndState.disableEUFusion.value);
}
HWTEST2_F(CommandListTests, GivenKernelWithDpasWhenUpdateStreamPropertiesForRegularCommandListsCalledAndLwsIsNonOddThenFusedEuIsNotDisabled, IsDG2) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    const ze_group_count_t launchKernelArgs = {3, 1, 1};
    kernel.groupSize[0] = 8;
    kernel.groupSize[1] = 1;
    kernel.groupSize[2] = 1;
    commandList->updateStreamPropertiesForRegularCommandLists(kernel, false, launchKernelArgs, false);
    EXPECT_FALSE(commandList->finalStreamState.frontEndState.disableEUFusion.value);
}

HWTEST2_F(CommandListTests, GivenKernelWithDpasWhenUpdateStreamPropertiesForRegularCommandListsCalledAndLwsOneAndDispatchIsIndirectThenFusedEuIsDisabled, IsDG2) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    const ze_group_count_t launchKernelArgs = {4, 1, 1};
    kernel.groupSize[0] = 1;
    kernel.groupSize[1] = 1;
    kernel.groupSize[2] = 1;
    commandList->updateStreamPropertiesForRegularCommandLists(kernel, false, launchKernelArgs, true);
    EXPECT_TRUE(commandList->finalStreamState.frontEndState.disableEUFusion.value);
}

HWTEST2_F(CommandListTests, GivenKernelWithDpasWhenUpdateStreamPropertiesForFlushTaskDispatchFlagsCalledAndLwsIsOddThenFusedEuIsDisabled, IsDG2) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    const ze_group_count_t launchKernelArgs = {3, 1, 1};
    kernel.groupSize[0] = 7;
    kernel.groupSize[1] = 1;
    kernel.groupSize[2] = 1;
    commandList->updateStreamPropertiesForFlushTaskDispatchFlags(kernel, false, launchKernelArgs, false);
    EXPECT_TRUE(commandList->requiredStreamState.frontEndState.disableEUFusion.value);
}
HWTEST2_F(CommandListTests, GivenKernelWithDpasWhenUpdateStreamPropertiesForFlushTaskDispatchFlagsCalledAndLwsIsNonOddThenFusedEuIsNotDisabled, IsDG2) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    const ze_group_count_t launchKernelArgs = {3, 1, 1};
    kernel.groupSize[0] = 8;
    kernel.groupSize[1] = 1;
    kernel.groupSize[2] = 1;
    commandList->updateStreamPropertiesForFlushTaskDispatchFlags(kernel, false, launchKernelArgs, false);
    EXPECT_FALSE(commandList->requiredStreamState.frontEndState.disableEUFusion.value);
}

HWTEST2_F(CommandListTests, GivenKernelWithDpasWhenUpdateStreamPropertiesForFlushTaskDispatchFlagsCalledAndLwsOneAndDispatchIsIndirectThenFusedEuIsDisabled, IsDG2) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    const ze_group_count_t launchKernelArgs = {4, 1, 1};
    kernel.groupSize[0] = 1;
    kernel.groupSize[1] = 1;
    kernel.groupSize[2] = 1;
    commandList->updateStreamPropertiesForFlushTaskDispatchFlags(kernel, false, launchKernelArgs, true);
    EXPECT_TRUE(commandList->requiredStreamState.frontEndState.disableEUFusion.value);
}

} // namespace ult
} // namespace L0
