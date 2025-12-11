/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include <level_zero/driver_experimental/zex_cmdlist.h>

#include <ranges>

namespace L0 {
namespace ult {

using HostFunctionTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionTests, givenRegularCommandListWhenZexCommandListAppendHostFunctionIsCalledThenSuccessIsReturned) {

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    auto result = zeCommandListAppendHostFunction(commandList->toHandle(), pHostFunction, pUserData, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(HostFunctionTests, givenCopyCommandListWhenZexCommandListAppendHostFunctionIsCalledThenSuccessIsReturned) {

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    auto result = zeCommandListAppendHostFunction(commandList->toHandle(), pHostFunction, pUserData, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(HostFunctionTests, givenSynchronousImmediateCommandListWhenZexCommandListAppendHostFunctionIsCalledThenSuccessIsReturned) {

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    auto result = zeCommandListAppendHostFunction(commandList->toHandle(), pHostFunction, pUserData, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(HostFunctionTests, givenAsynchronousImmediateCommandListWhenZexCommandListAppendHostFunctionIsCalledThenSuccessIsReturned) {

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    auto result = zeCommandListAppendHostFunction(commandList->toHandle(), pHostFunction, pUserData, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(HostFunctionTests, givenInvalidWaitEventsHandleWhenAppendHostFunctionIsCalledThenInvalidArgumentErrorIsReturned) {

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    CmdListHostFunctionParameters parameters{.relaxedOrderingDispatch = false};

    uint32_t numWaitEvents = 1;
    ze_event_handle_t *phWaitEvents = nullptr;

    returnValue = commandList->appendHostFunction(pHostFunction, pUserData, nullptr, nullptr, numWaitEvents, phWaitEvents, parameters);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

HWTEST_F(HostFunctionTests, givenWaitEventWhenAppendHostFunctionIsCalledThenSemaphoreWaitIsProgrammedCorectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    std::unique_ptr<::L0::EventPool> eventPool(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    std::unique_ptr<::L0::Event> event(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
    ze_event_handle_t hEventHandle = event->toHandle();
    uint32_t numWaitEvents = 1;

    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    CmdListHostFunctionParameters parameters{};

    returnValue = commandList->appendHostFunction(pHostFunction, pUserData, nullptr, nullptr, numWaitEvents, &hEventHandle, parameters);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto usedSpace = commandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpace);

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(-1), cmd->getSemaphoreDataDword());
    auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

    uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

    EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
}

HWTEST_F(HostFunctionTests, givenOOQCmdListAndCounterBasedEventThenAppendHostFunctionIsCalledThenInvalidArgumentErrorIsReturned) {

    ze_result_t result;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    ze_event_pool_counter_based_exp_desc_t counterBasedExtension = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
    counterBasedExtension.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;
    eventPoolDesc.pNext = &counterBasedExtension;
    ze_event_desc_t eventDesc = {};
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false)));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    CmdListHostFunctionParameters parameters{.relaxedOrderingDispatch = false};

    ze_command_queue_desc_t queueDesc = {};
    std::unique_ptr<Mock<CommandQueue>> queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::compute, 0u);

    result = cmdList.appendHostFunction(pHostFunction, pUserData, nullptr, event->toHandle(), 0, nullptr, parameters);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST_F(HostFunctionTests, givenRegularCmdListWhenDispatchHostFunctionIsCalledThenCommandsToPatchAreSet) {

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    commandList->dispatchHostFunction(pHostFunction, pUserData);

    ASSERT_EQ(2u, commandList->commandsToPatch.size());

    EXPECT_EQ(CommandToPatch::HostFunctionId, commandList->commandsToPatch[0].type);
    EXPECT_EQ(reinterpret_cast<uint64_t>(pHostFunction), commandList->commandsToPatch[0].baseAddress);
    EXPECT_EQ(reinterpret_cast<uint64_t>(pUserData), commandList->commandsToPatch[0].gpuAddress);
    EXPECT_NE(nullptr, commandList->commandsToPatch[0].pCommand);

    EXPECT_EQ(CommandToPatch::HostFunctionWait, commandList->commandsToPatch[1].type);
    EXPECT_NE(nullptr, commandList->commandsToPatch[1].pCommand);
}

class HostFunctionTestsImmediateCmdListTest : public HostFunctionTests,
                                              public ::testing::WithParamInterface<ze_command_queue_mode_t> {
};

HWTEST_P(HostFunctionTestsImmediateCmdListTest, givenImmediateCmdListWhenDispatchHostFunctionIscalledThenCorrectCommandsAreProgrammedAndHostFunctionWasInitializedInCsr) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto queueMode = GetParam();

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = queueMode;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue)));

    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    uint64_t hostFunctionAddress = reinterpret_cast<uint64_t>(pHostFunction);

    void *pUserData = reinterpret_cast<void *>(0xd'0000);
    uint64_t userDataAddress = reinterpret_cast<uint64_t>(pUserData);

    auto *cmdStream = commandList->commandContainer.getCommandStream();
    auto offset = cmdStream->getUsed();

    commandList->dispatchHostFunction(pHostFunction, pUserData);

    //  different csr
    auto csr = commandList->getCsr(false);

    auto *hostFunctionAllocation = csr->getHostFunctionStreamer().getHostFunctionIdAllocation();
    ASSERT_NE(nullptr, hostFunctionAllocation);

    auto hostFunctionIdAddress = csr->getHostFunctionStreamer().getHostFunctionIdGpuAddress();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(*cmdStream, offset);

    auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miStores.size());

    auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miWait.size());

    // program callback
    uint64_t expectedHostFunctionId = 1u;
    auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
    EXPECT_EQ(hostFunctionIdAddress, miStoreUserHostFunction->getAddress());
    EXPECT_EQ(getLowPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword0());
    EXPECT_EQ(getHighPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword1());
    EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

    // wait for completion
    auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[0]);
    EXPECT_EQ(hostFunctionIdAddress, miWaitTag->getSemaphoreGraphicsAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionStatus::completed), miWaitTag->getSemaphoreDataDword());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());

    *csr->getHostFunctionStreamer().getHostFunctionIdPtr() = expectedHostFunctionId;
    auto hostFunction = csr->getHostFunctionStreamer().getHostFunction();
    EXPECT_EQ(hostFunctionAddress, hostFunction.hostFunctionAddress);
    EXPECT_EQ(userDataAddress, hostFunction.userDataAddress);
}

INSTANTIATE_TEST_SUITE_P(HostFunctionTestsImmediateCmdListTestValues,
                         HostFunctionTestsImmediateCmdListTest,
                         ::testing::Values(ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                           ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS));

using HostFunctionsInOrderCmdListTests = InOrderCmdListFixture;

HWTEST_F(HostFunctionsInOrderCmdListTests, givenInOrderModeWhenAppendHostFunctionThenWaitAndSignalDependenciesAreProgrammed) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    auto eventPool = createEvents<FamilyType>(2, false);

    CmdListHostFunctionParameters parameters{};
    void *pHostFunction = reinterpret_cast<void *>(0xa'0000);
    void *pUserData = reinterpret_cast<void *>(0xd'0000);

    auto used1 = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    immCmdList->latestOperationRequiredNonWalkerInOrderCmdsChaining = false;

    auto used2 = cmdStream->getUsed();
    immCmdList->appendHostFunction(pHostFunction, pUserData, nullptr, events[1]->toHandle(), 0, nullptr, parameters);

    auto used3 = cmdStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList1,
                                                      ptrOffset(cmdStream->getCpuBase(), used1),
                                                      used2 - used1));

    GenCmdList cmdList2;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList2,
                                                      ptrOffset(cmdStream->getCpuBase(), used2),
                                                      used3 - used2));

    // wait for implicit dependencies
    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList2.begin(), cmdList2.end());
    ASSERT_NE(cmdList2.end(), itor);

    if (immCmdList->isQwordInOrderCounter()) {
        std::advance(itor, -2); // verify 2x LRI before semaphore and semaphore
    }

    auto inOrderExecInfo = immCmdList->inOrderExecInfo;
    auto expectedWaitCounter = 1u;
    ASSERT_TRUE(verifyInOrderDependency<FamilyType>(itor, expectedWaitCounter, inOrderExecInfo->getBaseDeviceAddress(), immCmdList->isQwordInOrderCounter(), false));

    // host function dispatched data
    auto storeDataImmIt1 = find<MI_STORE_DATA_IMM *>(itor, cmdList2.end());
    ASSERT_NE(cmdList2.end(), storeDataImmIt1);

    auto semaphoreWait2 = find<MI_SEMAPHORE_WAIT *>(storeDataImmIt1, cmdList2.end());
    ASSERT_NE(cmdList2.end(), semaphoreWait2);

    // verify signal event
    auto it = immCmdList->inOrderAtomicSignalingEnabled ? find<MI_ATOMIC *>(semaphoreWait2, cmdList2.end())
                                                        : find<MI_STORE_DATA_IMM *>(semaphoreWait2, cmdList2.end());

    auto gpuAddress = inOrderExecInfo->getBaseDeviceAddress();

    if (immCmdList->inOrderAtomicSignalingEnabled) {
        auto miAtomicCmd = genCmdCast<MI_ATOMIC *>(*it);
        EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getAtomicMemoryAddress(*miAtomicCmd));

    } else {
        auto miStoreCmd = genCmdCast<MI_STORE_DATA_IMM *>(*it);
        EXPECT_EQ(gpuAddress, miStoreCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), miStoreCmd->getStoreQword());
        EXPECT_EQ(2u, miStoreCmd->getDataDword0());
    }

    if (inOrderExecInfo->isHostStorageDuplicated()) {
        it = find<MI_STORE_DATA_IMM *>(++it, cmdList2.end());
        auto miStoreCmd = genCmdCast<MI_STORE_DATA_IMM *>(*it);
        auto hostAddress = reinterpret_cast<uint64_t>(inOrderExecInfo->getBaseHostAddress());
        EXPECT_EQ(hostAddress, miStoreCmd->getAddress());
        EXPECT_EQ(immCmdList->isQwordInOrderCounter(), miStoreCmd->getStoreQword());
        EXPECT_EQ(2u, miStoreCmd->getDataDword0());
    }

    EXPECT_EQ(1u, events[0]->inOrderExecSignalValue);
    EXPECT_EQ(2u, events[1]->inOrderExecSignalValue);
}

} // namespace ult
} // namespace L0
