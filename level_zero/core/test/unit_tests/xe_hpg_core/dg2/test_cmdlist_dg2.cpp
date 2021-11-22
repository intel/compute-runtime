/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/xe_hpg_core/dg2/cmdlist_dg2.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {
using CommandListTests = Test<DeviceFixture>;

HWTEST2_F(CommandListTests, givenDG2WithBSteppingWhenCreatingCommandListThenAdditionalStateBaseAddressCmdIsAdded, IsDG2) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    auto &hwInfo = *neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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

template <PRODUCT_FAMILY productFamily>
struct CommandListAdjustStateComputeMode : public WhiteBox<::L0::CommandListProductFamily<productFamily>> {
    CommandListAdjustStateComputeMode() : WhiteBox<::L0::CommandListProductFamily<productFamily>>(1) {}
    using ::L0::CommandListProductFamily<productFamily>::updateStreamProperties;
    using ::L0::CommandListProductFamily<productFamily>::finalStreamState;
};
HWTEST2_F(CommandListTests, GivenComputeModePropertiesWhenUpdateStreamPropertiesIsCalledTwiceThenAllFieldsChanged, IsDG2) {
    DebugManagerStateRestore restorer;
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    auto pCommandList = std::make_unique<CommandListAdjustStateComputeMode<productFamily>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x100;
    pCommandList->updateStreamProperties(kernel, false, false);
    EXPECT_TRUE(pCommandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
    EXPECT_TRUE(pCommandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x80;
    pCommandList->updateStreamProperties(kernel, false, false);
    EXPECT_TRUE(pCommandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    EXPECT_TRUE(pCommandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
}
} // namespace ult
} // namespace L0
