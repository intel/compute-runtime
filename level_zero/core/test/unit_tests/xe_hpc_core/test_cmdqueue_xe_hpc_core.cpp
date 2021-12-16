/*
 * Copyright (C) 2021-2022 Intel Corporation
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

struct CommandQueueCreateMultiOrdinalFixture : public DeviceFixture {
    void SetUp() {
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
        hwInfo.featureTable.flags.ftrCCSNode = true;
        hwInfo.featureTable.ftrBcsInfo = 0;
        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() {
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
};

using CommandQueueCommandsPvc = Test<DeviceFixture>;

HWTEST2_F(CommandQueueCommandsPvc, givenCommandQueueWhenExecutingCommandListsThenGlobalFenceAllocationIsResident, IsXeHpcCore) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);
    csr.createGlobalFenceAllocation();

    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, &csr, &desc);
    commandQueue->initialize(false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto globalFence = csr.getGlobalFenceAllocation();

    bool found = false;
    for (auto alloc : csr.copyOfAllocations) {
        if (alloc == globalFence) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsPvc, givenCommandQueueWhenExecutingCommandListsThenStateSystemMemFenceAddressCmdIsGenerated, IsXeHpcCore) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto globalFence = csr->getGlobalFenceAllocation();

    auto used = commandQueue->commandStream->getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandQueue->commandStream->getCpuBase(), used));

    auto itor = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itor);
    EXPECT_EQ(globalFence->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsPvc, givenCommandQueueWhenExecutingCommandListsForTheSecondTimeThenStateSystemMemFenceAddressCmdIsNotGenerated, IsXeHpcCore) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    auto used = commandQueue->commandStream->getUsed();
    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    auto sizeUsed2 = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), used), sizeUsed2));

    auto itor = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsPvc, givenLinkedCopyEngineOrdinalWhenCreatingThenSetAsCopyOnly, IsXeHpcCore) {
    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo.set(1, true);
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t result = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_queue_desc_t cmdQueueDesc = {};
    ze_command_queue_handle_t cmdQueue;

    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::LinkedCopy));

    result = zeCommandQueueCreate(hContext, testL0Device.get(), &cmdQueueDesc, &cmdQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdQ = L0::CommandQueue::fromHandle(cmdQueue);

    EXPECT_TRUE(cmdQ->peekIsCopyOnlyCommandQueue());

    cmdQ->destroy();
    L0::Context::fromHandle(hContext)->destroy();
}

HWTEST2_F(CommandQueueCommandsPvc, whenExecuteCommandListsIsCalledThenAdditionalCfeStatesAreCorrectlyProgrammed, IsXeHpcCore) {
    if (!XE_HPC_CORE::isXtTemporary(*defaultHwInfo)) {
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
    if constexpr (IsPVC::isMatched<productFamily>()) {
        hwInfo.platform.usRevId |= FamilyType::pvcBaseDieRevMask;
    }
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
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>{device, csr, &desc};
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
    auto commandListAB = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandListAB->initialize(device, NEO::EngineGroupType::CooperativeCompute, 0u);
    commandListAB->appendLaunchKernelWithParams(&defaultKernel, &threadGroupDimensions, nullptr, false, false, false);
    commandListAB->appendLaunchKernelWithParams(&cooperativeKernel, &threadGroupDimensions, nullptr, false, false, true);
    auto hCommandListAB = commandListAB->toHandle();

    auto commandListBA = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandListBA->initialize(device, NEO::EngineGroupType::CooperativeCompute, 0u);
    commandListBA->appendLaunchKernelWithParams(&cooperativeKernel, &threadGroupDimensions, nullptr, false, false, true);
    commandListBA->appendLaunchKernelWithParams(&defaultKernel, &threadGroupDimensions, nullptr, false, false, false);
    auto hCommandListBA = commandListBA->toHandle();

    // Set state B
    csr->getStreamProperties().frontEndState.setProperties(true, false, false, *NEO::defaultHwInfo);
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
    csr->getStreamProperties().frontEndState.setProperties(false, false, false, *NEO::defaultHwInfo);
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
