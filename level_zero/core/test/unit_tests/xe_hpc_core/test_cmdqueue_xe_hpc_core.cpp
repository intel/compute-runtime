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
#include "shared/test/common/test_macros/hw_test.h"

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

using CommandQueueCommandsXeHpc = Test<DeviceFixture>;

HWTEST2_F(CommandQueueCommandsXeHpc, givenCommandQueueWhenExecutingCommandListsThenGlobalFenceAllocationIsResident, IsXeHpcCore) {
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

HWTEST2_F(CommandQueueCommandsXeHpc, givenCommandQueueWhenExecutingCommandListsThenStateSystemMemFenceAddressCmdIsGenerated, IsXeHpcCore) {
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

HWTEST2_F(CommandQueueCommandsXeHpc, givenCommandQueueWhenExecutingCommandListsForTheSecondTimeThenStateSystemMemFenceAddressCmdIsNotGenerated, IsXeHpcCore) {
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

HWTEST2_F(CommandQueueCommandsXeHpc, givenLinkedCopyEngineOrdinalWhenCreatingThenSetAsCopyOnly, IsXeHpcCore) {
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

} // namespace ult
} // namespace L0
