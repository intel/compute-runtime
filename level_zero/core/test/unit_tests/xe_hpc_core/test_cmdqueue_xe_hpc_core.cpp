/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

struct CommandQueueCommandsXeHpc : Test<DeviceFixture> {
    CmdListMemoryCopyParams copyParams = {};
};

HWTEST2_F(CommandQueueCommandsXeHpc, givenCommandQueueWhenExecutingCommandListsThenGlobalFenceAllocationIsResident, IsXeHpcCore) {
    ze_command_queue_desc_t desc = {};
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);
    csr.createGlobalFenceAllocation();

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, &csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

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

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    auto globalFence = csr->getGlobalFenceAllocation();

    auto used = commandQueue->commandStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    auto usedSpaceAfter1stExecute = commandQueue->commandStream.getUsed();
    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    auto usedSpaceOn2ndExecute = commandQueue->commandStream.getUsed() - usedSpaceAfter1stExecute;

    GenCmdList cmdList;
    auto cmdBufferAddress = ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceAfter1stExecute);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdBufferAddress, usedSpaceOn2ndExecute));

    auto itor = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenDebugFlagWithLinkedEngineSetWhenCreatingCommandQueueThenOverrideEngineIndex, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    const uint32_t newIndex = 2;
    debugManager.flags.ForceBcsEngineIndex.set(newIndex);
    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    auto &engineGroups = testNeoDevice->getRegularEngineGroups();

    uint32_t expectedCopyOrdinal = 0;
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == EngineGroupType::linkedCopy) {
            expectedCopyOrdinal = i;
            break;
        }
    }

    bool queueCreated = false;
    bool hasMultiInstancedEngine = false;
    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            bool copyOrdinal = NEO::EngineHelper::isCopyOnlyEngineType(engineGroups[ordinal].engineGroupType);
            if (engineGroups[ordinal].engines.size() > 1 && copyOrdinal) {
                hasMultiInstancedEngine = true;
            }

            ze_command_queue_handle_t commandQueue = {};

            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t res = context->createCommandQueue(testL0Device.get(), &desc, &commandQueue);

            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            EXPECT_NE(nullptr, commandQueue);

            auto queue = whiteboxCast(L0::CommandQueue::fromHandle(commandQueue));

            if (copyOrdinal) {
                EXPECT_EQ(engineGroups[expectedCopyOrdinal].engines[newIndex - 1].commandStreamReceiver, queue->csr);
                queueCreated = true;
            } else {
                EXPECT_EQ(engineGroups[ordinal].engines[index].commandStreamReceiver, queue->csr);
            }

            queue->destroy();
        }
    }

    EXPECT_EQ(hasMultiInstancedEngine, queueCreated);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenDebugFlagWithInvalidIndexSetWhenCreatingCommandQueueThenReturnError, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    const uint32_t newIndex = 999;
    debugManager.flags.ForceBcsEngineIndex.set(newIndex);
    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    auto &engineGroups = testNeoDevice->getRegularEngineGroups();

    uint32_t expectedCopyOrdinal = 0;
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == EngineGroupType::linkedCopy) {
            expectedCopyOrdinal = i;
            break;
        }
    }

    ze_command_queue_handle_t commandQueue = {};

    ze_command_queue_desc_t desc = {};
    desc.ordinal = expectedCopyOrdinal;
    desc.index = 0;
    ze_result_t res = context->createCommandQueue(testL0Device.get(), &desc, &commandQueue);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenDebugFlagWithNonExistingIndexSetWhenCreatingCommandQueueThenReturnError, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    const uint32_t newIndex = 1;
    debugManager.flags.ForceBcsEngineIndex.set(newIndex);
    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    auto &engineGroups = testNeoDevice->getRegularEngineGroups();

    uint32_t expectedCopyOrdinal = 0;
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == EngineGroupType::copy) {
            expectedCopyOrdinal = i;
            break;
        }
    }

    ze_command_queue_handle_t commandQueue = {};

    ze_command_queue_desc_t desc = {};
    desc.ordinal = expectedCopyOrdinal;
    desc.index = 0;
    ze_result_t res = context->createCommandQueue(testL0Device.get(), &desc, &commandQueue);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenDebugFlagWithMainEngineSetWhenCreatingCommandQueueThenOverrideEngineIndex, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    const uint32_t newIndex = 0;
    debugManager.flags.ForceBcsEngineIndex.set(newIndex);
    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    auto &engineGroups = testNeoDevice->getRegularEngineGroups();

    uint32_t expectedCopyOrdinal = 0;
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == EngineGroupType::copy) {
            expectedCopyOrdinal = i;
            break;
        }
    }

    bool queueCreated = false;
    bool hasMultiInstancedEngine = false;
    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            bool copyOrdinal = NEO::EngineHelper::isCopyOnlyEngineType(engineGroups[ordinal].engineGroupType);
            if (engineGroups[ordinal].engines.size() > 1 && copyOrdinal) {
                hasMultiInstancedEngine = true;
            }

            ze_command_queue_handle_t commandQueue = {};

            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t res = context->createCommandQueue(testL0Device.get(), &desc, &commandQueue);

            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            EXPECT_NE(nullptr, commandQueue);

            auto queue = whiteboxCast(L0::CommandQueue::fromHandle(commandQueue));

            if (copyOrdinal) {
                EXPECT_EQ(engineGroups[expectedCopyOrdinal].engines[newIndex].commandStreamReceiver, queue->csr);
                queueCreated = true;
            } else {
                EXPECT_EQ(engineGroups[ordinal].engines[index].commandStreamReceiver, queue->csr);
            }

            queue->destroy();
        }
    }

    EXPECT_EQ(hasMultiInstancedEngine, queueCreated);
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

    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::linkedCopy));

    result = zeCommandQueueCreate(hContext, testL0Device.get(), &cmdQueueDesc, &cmdQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdQ = L0::CommandQueue::fromHandle(cmdQueue);

    EXPECT_TRUE(cmdQ->peekIsCopyOnlyCommandQueue());

    cmdQ->destroy();
    L0::Context::fromHandle(hContext)->destroy();
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateThenSplitCmdQAreCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    ASSERT_NE(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);

    std::unique_ptr<L0::CommandList> commandList2(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList2);
    EXPECT_NE(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);

    commandList->destroy();
    commandList.release();
    EXPECT_NE(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);

    commandList2->destroy();
    commandList2.release();
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenNotAllBlittersAvailableWhenCreateImmediateThenSplitCmdQAreNotCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b110001001;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);

    commandList->destroy();
    commandList.release();
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenNotAllBlittersAvailableAndSplitBcsMaskSetWhenCreateImmediateThenSplitCmdQAreCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.SplitBcsMask.set(0b010001000);
    debugManager.flags.SplitBcsRequiredEnginesCount.set(2);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b110101001;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 2u);

    commandList->destroy();
    commandList.release();
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndSplitBcsMaskWhenCreateImmediateThenGivenCountOfSplitCmdQAreCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.SplitBcsMask.set(0b11001);
    debugManager.flags.SplitBcsRequiredEnginesCount.set(3);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 3u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateThenInitializeCmdQsOnce, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    debugManager.flags.SplitBcsMask.set(0b11001);
    debugManager.flags.SplitBcsRequiredEnginesCount.set(3);
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 3u);

    debugManager.flags.SplitBcsMask.set(0b110);
    debugManager.flags.SplitBcsRequiredEnginesCount.set(2);
    std::unique_ptr<L0::CommandList> commandList2(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList2);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 3u);

    commandList->destroy();
    commandList.release();
    commandList2->destroy();
    commandList2.release();
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateInternalThenSplitCmdQArenotCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, true, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyWhenCreateImmediateLinkedThenSplitCmdQAreNotCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::linkedCopy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopySetZeroWhenCreateImmediateThenSplitCmdQAreNotCreated, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(0);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, testL0Device.get(), &cmdQueueDesc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyWithSizeLessThanFourMBThenDoNotSplit, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyFromHostToHostThenDoNotSplit, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc,
                          size, alignment, &srcPtr);
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyHostptrDisabledAndImmediateCommandListWhenAppendingMemoryCopyFromNonUsmHostToHostThenDoNotSplit, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.SplitBcsCopyHostptr.set(0);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyHostptrDisabledAndImmediateCommandListWhenAppendingMemoryCopyFromHostToNonUsmHostThenDoNotSplit, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.SplitBcsCopyHostptr.set(0);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &srcPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    context->freeMem(srcPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyFromNonUsmHostToHostThenDoSplit, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &dstPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyFromHostToNonUsmHostThenDoSplit, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr = reinterpret_cast<void *>(0x1234);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &srcPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    context->freeMem(srcPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyFromDeviceToDeviceThenDoNotSplit, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenFlushTaskSubmissionEnabledAndSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;
    int client;
    ultCsr->registerClient(&client);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenFlushTaskSubmissionEnabledAndSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyThenUpdateTaskCount, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;
    int client;
    ultCsr->registerClient(&client);

    auto cmdStream0 = commandList0->getCmdContainer().getCommandStream();
    auto offset0 = cmdStream0->getUsed();

    auto cmdList2 = static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2]);
    auto cmdStream2 = cmdList2->getCmdContainer().getCommandStream();
    auto offset2 = cmdStream2->getUsed();

    auto cmdList3 = static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3]);
    auto cmdStream3 = cmdList3->getCmdContainer().getCommandStream();
    auto offset3 = cmdStream3->getUsed();

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto csr0 = commandList0->getCsr(false);
    auto csr2 = cmdList2->getCsr(false);
    auto csr3 = cmdList3->getCsr(false);

    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(csr3->peekTaskCount(), 1u);

    GenCmdList genCmdList0;
    GenCmdList genCmdList2;
    GenCmdList genCmdList3;

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList0, ptrOffset(cmdStream0->getCpuBase(), offset0), (cmdStream0->getUsed() - offset0)));
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList2, ptrOffset(cmdStream2->getCpuBase(), offset2), (cmdStream2->getUsed() - offset2)));
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(genCmdList3, ptrOffset(cmdStream3->getCpuBase(), offset3), (cmdStream3->getUsed() - offset3)));

    auto findTagUpdate = [&](GenCmdList &genCmdList, CommandStreamReceiver *csr) {
        auto found = std::find_if(genCmdList.cbegin(), genCmdList.cend(), [&](void *cmd) {
            if (auto cmdHw = genCmdCast<MI_FLUSH_DW *>(cmd)) {
                return cmdHw->getDestinationAddress() == csr->getTagAllocation()->getGpuAddress();
            }
            return false;
        });

        return (found != genCmdList.cend());
    };

    EXPECT_TRUE(findTagUpdate(genCmdList0, csr0));
    EXPECT_TRUE(findTagUpdate(genCmdList2, csr2));
    EXPECT_TRUE(findTagUpdate(genCmdList3, csr3));

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSyncCmdListAndSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;
    int client;
    ultCsr->registerClient(&client);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    EXPECT_EQ(ultCsr->waitForCompletionWithTimeoutTaskCountCalled, 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenFlushTaskSubmissionEnabledAndSplitBcsCopyAndImmediateCommandListWithRelaxedOrderingWhenAppendingMemoryCopyThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.set(1);
    debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.set(0);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);

    commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);

    directSubmission->relaxedOrderingEnabled = false;

    commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);

    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenRelaxedOrderingNotAllowedWhenDispatchSplitThenUseSemaphores, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

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
    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));
    ultCsr->recordFlushedBatchBuffer = true;
    EXPECT_EQ(0u, ultCsr->getNumClients());

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, ultCsr->getNumClients());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->getCsr(false)->peekTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->getCsr(false)->peekTaskCount(), 1u);
    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);

    uint32_t semaphoresFound = 0;
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandList0->getCmdContainer().getCommandStream()->getCpuBase(), commandList0->getCmdContainer().getCommandStream()->getUsed()));

    for (auto &cmd : cmdList) {
        if (genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(cmd)) {
            semaphoresFound++;
        }
    }

    EXPECT_EQ(2u, semaphoresFound);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyD2HThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyH2DThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    constexpr size_t alignment = 4096u;
    constexpr size_t size = 8 * MemoryConstants::megaByte;
    void *srcPtr;
    void *dstPtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(),
                            &deviceDesc,
                            size, alignment, &dstPtr);
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &srcPtr);

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyRegionThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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

    auto result = commandList0->appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyWithEventThenSuccessIsReturnedAndMiFlushProgrammed, IsXeHpcCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    std::unique_ptr<EventPool> eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<Event> event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

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

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, event->toHandle(), 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandList0->getCmdContainer().getCommandStream()->getCpuBase(), commandList0->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyAfterBarrierThenSuccessIsReturnedAndMiSemaphoresProgrammed, IsXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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
    *whiteBoxCmdList->getCsr(false)->getBarrierCountTagAddress() = 0u;
    whiteBoxCmdList->getCsr(false)->getNextBarrierCount();

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandList0->getCmdContainer().getCommandStream()->getCpuBase(), commandList0->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = cmdList.begin();
    for (uint32_t i = 0u; i < static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size() * 2; ++i) {
        itor = find<MI_SEMAPHORE_WAIT *>(itor, cmdList.end());
        EXPECT_NE(cmdList.end(), itor);
    }

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingMemoryCopyWithProfilingEventThenSuccessIsReturnedAndMiSemaphoresProgrammedBeforeProfiling, IsXeHpcCore) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    std::unique_ptr<EventPool> eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<Event> event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    auto result = commandList0->appendMemoryCopy(dstPtr, srcPtr, size, event->toHandle(), 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandList0->getCmdContainer().getCommandStream()->getCpuBase(), commandList0->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAllocateNewEventsForSplitThenEventsAreManagedProperly, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 0u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createFromPool(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 4u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 6u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createFromPool(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 8u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 12u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createFromPool(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 3u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 12u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 3u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 6u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenObtainEventsForSplitThenReuseEventsIfMarkerIsSignaled, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 0u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createFromPool(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 4u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 6u);

    auto ret = static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.obtainForSplit(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_EQ(ret, 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 8u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 12u);

    static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker[1]->hostSignal(false);

    ret = static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.obtainForSplit(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_EQ(ret, 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 8u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 12u);

    NEO::debugManager.flags.OverrideEventSynchronizeTimeout.set(0);
    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(0);

    auto memoryManager = reinterpret_cast<MockMemoryManager *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->device.getDriverHandle()->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    ret = static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.obtainForSplit(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_EQ(ret, 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 1u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 8u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 2u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 12u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenOOMAndObtainEventsForSplitThenNulloptIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 0u);

    NEO::debugManager.flags.OverrideEventSynchronizeTimeout.set(0);
    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(0);

    auto memoryManager = reinterpret_cast<MockMemoryManager *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->device.getDriverHandle()->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    auto ret = static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.obtainForSplit(Context::fromHandle(commandList0->getCmdListContext()), 12);

    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.pools.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.marker.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.subcopy.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.barrier.size(), 0u);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->events.createdFromLatestPool, 0u);
}

HWTEST2_F(CommandQueueCommandsXeHpc, givenSplitBcsCopyAndImmediateCommandListWhenAppendingPageFaultCopyThenSuccessIsReturned, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);

    ze_result_t returnValue;
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = 0b111111111;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto testNeoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    auto testL0Device = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), testNeoDevice, false, &returnValue));

    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(testNeoDevice->getEngineGroupIndexFromEngineGroupType(NEO::EngineGroupType::copy));

    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               testL0Device.get(),
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    EXPECT_EQ(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists.size(), 4u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 0u);

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
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[0])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[1])->cmdQImmediate->getTaskCount(), 0u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[2])->cmdQImmediate->getTaskCount(), 1u);
    EXPECT_EQ(static_cast<WhiteBox<CommandList> *>(static_cast<DeviceImp *>(testL0Device.get())->bcsSplit->cmdLists[3])->cmdQImmediate->getTaskCount(), 1u);

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

} // namespace ult
} // namespace L0
