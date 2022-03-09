/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandQueueCommandsPvc = Test<DeviceFixture>;

PVCTEST_F(CommandQueueCommandsPvc, whenExecuteCommandListsIsCalledThenAdditionalCfeStatesAreCorrectlyProgrammed) {
    if (!PVC::isXtTemporary(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    DebugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto hwInfo = *NEO::defaultHwInfo;
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto bStep = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    if (bStep != CommonConstants::invalidStepping) {
        hwInfo.platform.usRevId = bStep;
    }

    hwInfo.platform.usRevId |= FamilyType::pvcBaseDieRevMask;
    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueueHw<IGFX_XE_HPC_CORE>{device, csr, &desc};
    commandQueue->initialize(false, false);

    Mock<::L0::Kernel> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::Kernel> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
    cooperativeKernel.setGroupSize(1, 1, 1);

    ze_group_count_t threadGroupDimensions{1, 1, 1};
    auto commandListAB = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    commandListAB->initialize(device, NEO::EngineGroupType::CooperativeCompute, 0u);
    commandListAB->appendLaunchKernelWithParams(&defaultKernel, &threadGroupDimensions, nullptr, false, false, false);
    commandListAB->appendLaunchKernelWithParams(&cooperativeKernel, &threadGroupDimensions, nullptr, false, false, true);
    auto hCommandListAB = commandListAB->toHandle();

    auto commandListBA = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<IGFX_XE_HPC_CORE>>>();
    commandListBA->initialize(device, NEO::EngineGroupType::CooperativeCompute, 0u);
    commandListBA->appendLaunchKernelWithParams(&cooperativeKernel, &threadGroupDimensions, nullptr, false, false, true);
    commandListBA->appendLaunchKernelWithParams(&defaultKernel, &threadGroupDimensions, nullptr, false, false, false);
    auto hCommandListBA = commandListBA->toHandle();

    // Set state B
    csr->getStreamProperties().frontEndState.setProperties(true, false, false, false, *NEO::defaultHwInfo);
    // Execute command list AB
    commandQueue->executeCommandLists(1, &hCommandListAB, nullptr, false);

    // Expect state A programmed by command queue
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandQueue->commandStream->getCpuBase(), commandQueue->commandStream->getUsed()));
    auto cfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, cfeStates.size());
    EXPECT_EQ(false, genCmdCast<CFE_STATE *>(*cfeStates[0])->getComputeDispatchAllWalkerEnable());

    // Expect state B programmed by command list
    cmdList.clear();
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandListAB->commandContainer.getCommandStream()->getCpuBase(), commandListAB->commandContainer.getCommandStream()->getUsed()));
    cfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, cfeStates.size());
    EXPECT_EQ(true, genCmdCast<CFE_STATE *>(*cfeStates[0])->getComputeDispatchAllWalkerEnable());

    // Set state A
    csr->getStreamProperties().frontEndState.setProperties(false, false, false, false, *NEO::defaultHwInfo);
    // Execute command list BA
    commandQueue->executeCommandLists(1, &hCommandListBA, nullptr, false);

    // Expect state B programmed by command queue
    cmdList.clear();
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandQueue->commandStream->getCpuBase(), commandQueue->commandStream->getUsed()));
    cfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(2u, cfeStates.size());
    EXPECT_EQ(false, genCmdCast<CFE_STATE *>(*cfeStates[0])->getComputeDispatchAllWalkerEnable());
    EXPECT_EQ(true, genCmdCast<CFE_STATE *>(*cfeStates[1])->getComputeDispatchAllWalkerEnable());

    // Expect state A programmed by command list
    cmdList.clear();
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandListBA->commandContainer.getCommandStream()->getCpuBase(), commandListBA->commandContainer.getCommandStream()->getUsed()));
    cfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, cfeStates.size());
    EXPECT_EQ(false, genCmdCast<CFE_STATE *>(*cfeStates[0])->getComputeDispatchAllWalkerEnable());

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
