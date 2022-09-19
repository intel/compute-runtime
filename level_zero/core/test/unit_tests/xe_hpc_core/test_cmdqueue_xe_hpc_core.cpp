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

    auto used = commandQueue->commandStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandQueue->commandStream.getCpuBase(), used));

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
    auto usedSpaceAfter1stExecute = commandQueue->commandStream.getUsed();
    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    auto usedSpaceOn2ndExecute = commandQueue->commandStream.getUsed() - usedSpaceAfter1stExecute;

    GenCmdList cmdList;
    auto cmdBufferAddress = ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceAfter1stExecute);
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdBufferAddress, usedSpaceOn2ndExecute));

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

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateThenSplitCmdQAreCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_NE(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 0u);

    std::unique_ptr<L0::CommandList> commandList2(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList2);
    EXPECT_NE(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 0u);

    commandList->destroy();
    commandList.release();
    EXPECT_NE(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 0u);

    commandList2->destroy();
    commandList2.release();
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndSplitBcsMaskWhenCreateImmediateThenGivenCountOfSplitCmdQAreCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);
    DebugManager.flags.SplitBcsMask.set(0b11001);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 3u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateThenInitializeCmdQsOnce, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    DebugManager.flags.SplitBcsMask.set(0b11001);
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 3u);

    DebugManager.flags.SplitBcsMask.set(0b110);
    std::unique_ptr<L0::CommandList> commandList2(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList2);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 3u);

    commandList->destroy();
    commandList.release();
    commandList2->destroy();
    commandList2.release();
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateInternalThenSplitCmdQArenotCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, true, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateLinkedThenSplitCmdQAreNotCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::LinkedCopy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopySetZeroWhenCreateImmediateThenSplitCmdQAreNotCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(0);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyWithSizeLessThanFourMBThenDoNotSplit, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 4 * MemoryConstants::megaByte - 1;
    void *srcPtr;
    void *dstPtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &srcPtr);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyFromHostToHostThenDoNotSplit, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc,
                          size, alignment, &srcPtr);
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyFromDeviceToDeviceThenDoNotSplit, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &srcPtr);
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyThenSuccessIsReturned, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &srcPtr);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyRegionThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &srcPtr);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);
    ze_copy_region_t region = {2, 1, 1, 4 * MemoryConstants::megaByte, 1, 1};

    auto result = commandList0->appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyWithEventThenSuccessIsReturnedAndMiFlushProgrammed, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    std::unique_ptr<EventPool> eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<Event> event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &srcPtr);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, event->toHandle(), 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 1u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, commandList0->commandContainer.getCommandStream()->getCpuBase(), commandList0->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAllocateNewEventsForSplitThenEventsAreManagedProperly, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 0u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.allocateNew(Context::fromHandle(commandList0->hContext), 10);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 4u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 5u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.allocateNew(Context::fromHandle(commandList0->hContext), 10);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 8u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 10u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.allocateNew(Context::fromHandle(commandList0->hContext), 10);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 3u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 12u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 5u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenObtainEventsForSplitThenReuseEventsIfMarkerIsSignaled, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 0u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.allocateNew(Context::fromHandle(commandList0->hContext), 10);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 4u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 5u);

    auto ret = static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.obtainForSplit(Context::fromHandle(commandList0->hContext), 10);

    EXPECT_EQ(ret, 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 8u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 10u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker[1]->hostSignal();

    ret = static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.obtainForSplit(Context::fromHandle(commandList0->hContext), 10);

    EXPECT_EQ(ret, 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.marker.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.subcopy.size(), 8u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.events.createdFromLatestPool, 10u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingPageFaultCopyThenSuccessIsReturned, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::Copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs.size(), 4u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &srcPtr);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendPageFaultCopy(testL0Device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(dstPtr)->gpuAllocations.getDefaultGraphicsAllocation(),
                                                    testL0Device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(srcPtr)->gpuAllocations.getDefaultGraphicsAllocation(),
                                                    size,
                                                    false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[0])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[1])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[2])->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<CommandQueueImp *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit.cmdQs[3])->getTaskCount(), 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

} // namespace ult
} // namespace L0
