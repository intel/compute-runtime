/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/core/ze_graph_api_entrypoints.h"
#include "level_zero/api/internal/l0_cmdlist.h"
#include "level_zero/api/internal/l0_event.h"
#include "level_zero/api/internal/l0_graph.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/core/test/unit_tests/experimental/test_graph_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_graph.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_cmdlist.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_load_register_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_semaphore_wait_hw.h"
#include "level_zero/driver_experimental/zex_graph.h"
#include "level_zero/ze_api.h"

#include <array>
#include <unordered_set>

using namespace NEO;

namespace L0 {

namespace ult {

using GraphTestCaptureRestrictions = Test<GraphFixture>;
using GraphTestApiPauseResume = Test<GraphFixture>;

struct GraphPatchPreambleBarrierFixture : public GraphFixture {
    void setUp() {
        debugManager.flags.OverrideGraphPatchPreambleBarrierSupport.set(1);
        debugManager.flags.ContextGroupSize.set(64);
        GraphFixture::setUp();
    }
};
using GraphPatchPreambleBarrierTest = Test<GraphPatchPreambleBarrierFixture>;

struct CapturingInternalExecCmdList : Mock<CommandList> {
    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CmdListWaitEventParameters &waitEventsParameters) override {
        appendWaitOnEventsCalled++;
        waitedEvents.clear();
        if ((numEvents > 0u) && (phEvent != nullptr)) {
            waitedEvents.insert(waitedEvents.end(), phEvent, phEvent + numEvents);
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent, CmdListSignalEventParameters &signalEventParameters) override {
        appendSignalEventCalled++;
        signaledEvents.push_back(hEvent);
        return ZE_RESULT_SUCCESS;
    }

    uint32_t appendWaitOnEventsCalled = 0;
    uint32_t appendSignalEventCalled = 0;
    std::vector<ze_event_handle_t> waitedEvents;
    std::vector<ze_event_handle_t> signaledEvents;
};

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenGraphUsesExternalCbEventOn32bSemWaitPlatformWhenPreambleCounterOverflowsAtGraphExecutionThenPatchPreambleDispatchesCounterPostSyncWithOffsetAddress) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = PIPE_CONTROL::POST_SYNC_OPERATION;

    if constexpr (FamilyType::isQwordInOrderCounter) {
        GTEST_SKIP();
    }

    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();
    auto whiteBoxCmdQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(commandList.get())->cmdQImmediate);

    size_t tagSize = device->getDeviceInOrderCounterAllocator()->getTagSize();
    size_t offset = tagSize / 2;

    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    ze_event_handle_t eventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    auto event = L0::Event::fromHandle(eventHandle);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandle, 0U, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*srcGraph.get());
    auto execGraphHandle = execGraph.toHandle();

    whiteBoxCmdQueue->patchPreambleCounter.counter = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());

    auto cmdBufferCpuBase = commandList->getCmdContainer().getCommandStream()->getCpuBase();

    auto sizeBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr));
    auto sizeAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    auto cbEventContainer = execGraph.getExternalCbEventInfoContainer().get();
    auto &rootHostPatchPreambleData = cbEventContainer->getPreambleData(nullptr);
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.counter, rootHostPatchPreambleData.counter());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeGpuAddress, rootHostPatchPreambleData.hostGpuAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.deviceNodeGpuAddress + offset, rootHostPatchPreambleData.deviceGpuAddress());

    auto counter = whiteBoxCmdQueue->patchPreambleCounter.counter;
    auto lowerCounter = getLowPart(counter);
    auto hostGpuAddress = whiteBoxCmdQueue->patchPreambleCounter.hostNodeGpuAddress;
    auto deviceGpuAddress = whiteBoxCmdQueue->patchPreambleCounter.deviceNodeGpuAddress + offset;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdBufferCpuBase, sizeBefore),
        sizeAfter - sizeBefore));

    bool foundHostPostSyncWithCounter = false;
    bool foundDevicePostSyncWithCounter = false;

    auto pipeControlCmds = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControlCmds.size());
    for (auto &pipeControlCmd : pipeControlCmds) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlCmd);
        auto actualAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA &&
            hostGpuAddress == actualAddress &&
            pipeControl->getImmediateData() == counter) {
            foundHostPostSyncWithCounter = true;
        }
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA &&
            deviceGpuAddress == actualAddress &&
            pipeControl->getImmediateData() == lowerCounter) {
            foundDevicePostSyncWithCounter = true;
        }
        if (foundDevicePostSyncWithCounter && foundHostPostSyncWithCounter) {
            break;
        }
    }
    EXPECT_TRUE(foundHostPostSyncWithCounter);
    EXPECT_TRUE(foundDevicePostSyncWithCounter);
    srcGraph.reset();
    event->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenExecutableGraphWhenSubmittingItToCommandListThenPatchPreambleIsUsed) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    uint32_t bbStartDwordBuffer[alignUp(sizeof(MI_BATCH_BUFFER_START) / sizeof(uint32_t), 2)] = {0};

    GraphsCleanupGuard graphCleanup;

    auto immCmdListHw = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(L0::CommandList::fromHandle(immCmdListHandle));

    Graph srcGraph(context, true, false);
    immCmdListHw->setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(*immCmdListHw, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0U, nullptr));
    srcGraph.stopCapturing();
    immCmdListHw->setGraphCaptureTarget(nullptr);

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    auto cmdStream = immCmdListHw->getCmdContainer().getCommandStream();

    void *immListCpuBase = cmdStream->getCpuBase();
    auto usedSpaceBefore = cmdStream->getUsed();
    auto res = execGraph.execute(immCmdListHw, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto usedSpaceAfter = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(immListCpuBase, usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LT(1u, sdiCmds.size());

    size_t bbStartIdx = 0;
    for (auto &sdiCmd : sdiCmds) {
        auto storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmd);

        bbStartDwordBuffer[bbStartIdx] = storeDataImm->getDataDword0();
        bbStartIdx++;
        if (storeDataImm->getStoreQword()) {
            bbStartDwordBuffer[bbStartIdx] = storeDataImm->getDataDword1();
            bbStartIdx++;
        }
    }

    MI_BATCH_BUFFER_START *chainBackBbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(bbStartDwordBuffer);
    ASSERT_NE(nullptr, chainBackBbStartCmd);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenUpdatedNodeCommandListWhenExecutingExecutableGraphThenCommandListIsClosedOnlyOnce) {
    GraphsCleanupGuard graphCleanup;

    Graph srcGraph(context, true, false);
    immCmdList->setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(*immCmdList, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0U, nullptr));
    srcGraph.stopCapturing();
    immCmdList->setGraphCaptureTarget(nullptr);

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph));
    Mock<CommandList> updatedCommandList;
    execGraph.updatedNodeCommandLists.push_back(&updatedCommandList);

    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(1u, updatedCommandList.closeCalled);
    EXPECT_TRUE(execGraph.updatedNodeCommandLists.empty());

    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(1u, updatedCommandList.closeCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenExecutableGraphWithMultipleEnginesWhenSubmittingItToCommandListThenSynchronizedPatchPreambleIsUsed) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.ContextGroupSize.set(64);

    ze_command_list_handle_t immCmdListHandleMultiEngine;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &immCmdListHandleMultiEngine));

    GraphsCleanupGuard graphCleanup;

    auto immCmdListHw = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(L0::CommandList::fromHandle(immCmdListHandleMultiEngine));
    Graph srcGraph(context, true, false);
    immCmdListHw->setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(*immCmdListHw, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandleMultiEngine, nullptr, 0U, nullptr));
    srcGraph.stopCapturing();
    immCmdListHw->setGraphCaptureTarget(nullptr);

    MockExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    ASSERT_NE(0u, execGraph.myCommandLists.size());
    auto whiteBoxCmdList = static_cast<WhiteBox<L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>> *>(immCmdListHw);
    EXPECT_TRUE(whiteBoxCmdList->cmdQImmediate->getSaveWaitForPreamble());

    void *dummyCpuPtr = reinterpret_cast<void *>(0x123AA000);
    MockGraphicsAllocation otherTagAllocation(dummyCpuPtr, immCmdListHw->getCsr(false)->getTagAllocation()->getGpuAddress() + 0x1000, 1);
    execGraph.myCommandLists[0]->saveLatestTagAndTaskCount(&otherTagAllocation, 0x123);

    auto cmdStream = immCmdListHw->getCmdContainer().getCommandStream();

    void *immListCpuBase = cmdStream->getCpuBase();
    auto usedSpaceBefore = cmdStream->getUsed();
    auto res = execGraph.execute(immCmdListHw, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto usedSpaceAfter = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(immListCpuBase, usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    auto semWaitCmds = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, semWaitCmds.size());

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(semWaitCmds[0], cmdList.end());
    ASSERT_NE(0u, sdiCmds.size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(immCmdListHandleMultiEngine));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            givenInOrderCmdListAndRegularCbEventRecordedWithApiGraphExternalFlagWhenInstantiateToGraphAndRunningOnOutsideGraphThenRecordAsExternalCbEventAndDispatchCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    auto event = L0::Event::fromHandle(eventHandle);

    ze_event_ext_flags_desc_t eventFlagsDesc = {ZE_STRUCTURE_TYPE_EVENT_EXT_FLAGS_DESC, nullptr, ZE_EVENT_EXT_FLAG_APPEND_GRAPH_EXTERNAL};

    std::unique_ptr<L0::Graph> outsideGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t outsideGraphHandle = outsideGraph->toHandle();

    // attach event to the outside graph, so when it runs, it will have its own patch preamble and in order counter when the outside graph is executed
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, outsideGraphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(commandListHandle, &eventFlagsDesc, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &outsideGraphHandle, nullptr));
    ExecutableGraph outsideExecGraph;
    auto ret = outsideExecGraph.instantiateFrom(*(outsideGraph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    auto outsideExecGraphHandle = outsideExecGraph.toHandle();

    auto &outsideExternalCbSignalEventContainer = outsideExecGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    ASSERT_EQ(1u, outsideExternalCbSignalEventContainer.size());
    auto itSignalOutside = std::find_if(outsideExternalCbSignalEventContainer.begin(),
                                        outsideExternalCbSignalEventContainer.end(),
                                        [event](const ExternalSignalCbEventInfo &info) { return info.event == event; });
    ASSERT_NE(outsideExternalCbSignalEventContainer.end(), itSignalOutside);
    EXPECT_TRUE((*itSignalOutside).apiRequiredSignalEvent);

    // now record wait and signal on event with external flag
    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEventsWithParameters(commandListHandle, &eventFlagsDesc, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(commandListHandle, &eventFlagsDesc, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));
    ExecutableGraph execGraph;
    ret = execGraph.instantiateFrom(*(srcGraph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    auto execGraphHandle = execGraph.toHandle();

    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    ASSERT_EQ(1u, externalCbSignalEventContainer.size());

    auto itSignal = std::find_if(externalCbSignalEventContainer.begin(),
                                 externalCbSignalEventContainer.end(),
                                 [event](const ExternalSignalCbEventInfo &info) { return info.event == event; });
    ASSERT_NE(externalCbSignalEventContainer.end(), itSignal);
    EXPECT_TRUE((*itSignal).apiRequiredSignalEvent);

    const auto &externalCbWaitEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbWaitEventInfos();
    ASSERT_EQ(1u, externalCbWaitEventContainer.size());
    const auto &info = externalCbWaitEventContainer[0];
    ASSERT_EQ(1u, info.waitEvents.size());
    EXPECT_EQ(eventHandle, info.waitEvents[0]);

    auto mclBackendCmdList = externalCbWaitEventContainer[0].executor->asMutable();
    ASSERT_NE(nullptr, mclBackendCmdList);
    auto whiteBoxMcl = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(mclBackendCmdList);
    ASSERT_NE(0u, whiteBoxMcl->eventMutations.size());
    auto &eventMutationPoint = whiteBoxMcl->eventMutations[0];
    ASSERT_EQ(1u, eventMutationPoint.waitEvents.size());
    auto waitExternalVariable = eventMutationPoint.waitEvents[0].eventVariable;
    EXPECT_TRUE(waitExternalVariable->getDesc().eventValue.apiRequiredExternal);
    EXPECT_TRUE(waitExternalVariable->getDesc().eventValue.isExternalFlag);
    ASSERT_EQ(2u, waitExternalVariable->getSemWaitList().size());

    // first sem wait is for patch preamble, second is for in order counter
    auto mutableSemWaitPatchPreamble = waitExternalVariable->getSemWaitList()[0];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWaitPatchPreambleCounter, mutableSemWaitPatchPreamble->getType());
    auto mutableSemWaitInOrderCounter = waitExternalVariable->getSemWaitList()[1];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWait, mutableSemWaitInOrderCounter->getType());

    // executing outside graph means the event is in state attached to the outside graph, so it should have attached its patch preamble and in its order counter
    ret = L0::zeCommandListAppendGraphExp(commandListHandle, outsideExecGraphHandle, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto outsidePatchPreambleCounter = event->getInOrderExecEventHelper().getPatchPreambleCounter();
    auto outsidePatchPreambleDeviceGpuAddress = event->getInOrderExecEventHelper().getPatchPreambleDeviceGpuAddress();

    auto outsideInOrderCounter = event->getInOrderExecBaseSignalValue();
    auto outsideInOrderCounterGpuAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();

    // now use it on target exec graph
    ret = L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    // graph uses command views to program and mutate commands
    auto semWaitCmdPatchPreamble = genCmdCast<MI_SEMAPHORE_WAIT *>(mutableSemWaitPatchPreamble->getCommandView());
    ASSERT_NE(nullptr, semWaitCmdPatchPreamble);
    EXPECT_EQ(outsidePatchPreambleDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdPatchPreamble));

    auto semWaitCmdInOrderCounter = genCmdCast<MI_SEMAPHORE_WAIT *>(mutableSemWaitInOrderCounter->getCommandView());
    ASSERT_NE(nullptr, semWaitCmdInOrderCounter);
    EXPECT_EQ(outsideInOrderCounterGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdInOrderCounter));

    if (waitExternalVariable->getDesc().eventValue.qwordIndirect) {
        // qword indirect means counter value is at lri commands, 1st pair is for patch preamble (1st is lower), 2nd pair is for in order counter (3rd is lower)
        ASSERT_EQ(4u, waitExternalVariable->getLoadRegImmList().size());
        auto mutableLriPatchPreambleLower = waitExternalVariable->getLoadRegImmList()[0];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter, mutableLriPatchPreambleLower->getType());
        auto lriCmdPatchPreambleLower = genCmdCast<MI_LOAD_REGISTER_IMM *>(mutableLriPatchPreambleLower->getCommandView());
        ASSERT_NE(nullptr, lriCmdPatchPreambleLower);
        EXPECT_EQ(outsidePatchPreambleCounter, lriCmdPatchPreambleLower->getDataDword());

        auto mutableLriInOrderCounterLower = waitExternalVariable->getLoadRegImmList()[2];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadCounter, mutableLriInOrderCounterLower->getType());
        auto lriCmdInOrderCounterLower = genCmdCast<MI_LOAD_REGISTER_IMM *>(mutableLriInOrderCounterLower->getCommandView());
        ASSERT_NE(nullptr, lriCmdInOrderCounterLower);
        EXPECT_EQ(outsideInOrderCounter, lriCmdInOrderCounterLower->getDataDword());
    } else {
        EXPECT_EQ(outsidePatchPreambleCounter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmdPatchPreamble));
        EXPECT_EQ(outsideInOrderCounter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmdInOrderCounter));
    }

    // and now since the event is used on the target exec graph, as a signal event, it should have its own patch preamble, which is different from the outside graph's
    // counter in this test was used on the same immediate command list, so the address will be the same, but the counter value should be greater
    auto execGraphPatchPreambleCounter = event->getInOrderExecEventHelper().getPatchPreambleCounter();
    auto execGraphPatchPreambleDeviceGpuAddress = event->getInOrderExecEventHelper().getPatchPreambleDeviceGpuAddress();
    EXPECT_EQ(execGraphPatchPreambleDeviceGpuAddress, outsidePatchPreambleDeviceGpuAddress);
    EXPECT_GT(execGraphPatchPreambleCounter, outsidePatchPreambleCounter);
    // verify that attaching the event as signal set correctly api required external graph flag
    EXPECT_TRUE(event->getApiRequiredGraphExternalEvent());

    outsideGraph.reset();
    srcGraph.reset();
    zeEventDestroy(eventHandle);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            givenRegularCbEventIsRecordedAsSignalInternalAndAsSignalWithApiGraphExternalFlagWhenInstantiateExecutableGraphThenCorrectEventIsSavedIntoExternalSignalStorage) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> rootCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    rootCommandList->setOrdinal(0);
    auto rootCommandListHandle = rootCommandList->toHandle();

    std::unique_ptr<L0::CommandList> forkCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    forkCommandList->setOrdinal(0);
    auto forkCommandListHandle = forkCommandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    auto event = L0::Event::fromHandle(eventHandle);

    ze_event_exp_flags_desc_t eventFlagsDesc = {ZE_STRUCTURE_TYPE_EVENT_EXP_FLAGS_DESC, nullptr, ZE_EVENT_EXP_FLAG_APPEND_GRAPH_EXTERNAL};

    std::unique_ptr<L0::Graph> graph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = graph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(rootCommandListHandle, graphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(rootCommandListHandle, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(forkCommandListHandle, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(forkCommandListHandle, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(rootCommandListHandle, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(rootCommandListHandle, &eventFlagsDesc, eventHandle)); // here it is used as external
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(rootCommandListHandle, &graphHandle, nullptr));
    ExecutableGraph execGraph;
    auto ret = execGraph.instantiateFrom(*(graph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    ASSERT_EQ(1u, externalCbSignalEventContainer.size());
    auto itSignalOutside = std::find_if(externalCbSignalEventContainer.begin(),
                                        externalCbSignalEventContainer.end(),
                                        [event](const ExternalSignalCbEventInfo &info) { return info.event == event; });
    ASSERT_NE(externalCbSignalEventContainer.end(), itSignalOutside);
    EXPECT_TRUE((*itSignalOutside).apiRequiredSignalEvent);

    graph.reset();
    zeEventDestroy(eventHandle);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            givenRegularCbEventIsRecordedAsWaitInternalAndAsWaitWithApiGraphExternalFlagWhenInstantiateExecutableGraphThenCorrectEventIsSavedIntoExternalWaitStorage) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> rootCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    rootCommandList->setOrdinal(0);
    auto rootCommandListHandle = rootCommandList->toHandle();

    std::unique_ptr<L0::CommandList> forkCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    forkCommandList->setOrdinal(0);
    auto forkCommandListHandle = forkCommandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    ze_event_exp_flags_desc_t eventFlagsDesc = {ZE_STRUCTURE_TYPE_EVENT_EXP_FLAGS_DESC, nullptr, ZE_EVENT_EXP_FLAG_APPEND_GRAPH_EXTERNAL};

    std::unique_ptr<L0::Graph> graph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = graph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(rootCommandListHandle, graphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEventsWithParameters(rootCommandListHandle, &eventFlagsDesc, 1, &eventHandle)); // here it is used as external
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(rootCommandListHandle, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(forkCommandListHandle, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(forkCommandListHandle, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(rootCommandListHandle, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(rootCommandListHandle, &graphHandle, nullptr));
    ExecutableGraph execGraph;
    auto ret = execGraph.instantiateFrom(*(graph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto &externalCbWaitEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbWaitEventInfos();
    ASSERT_EQ(1u, externalCbWaitEventContainer.size());
    auto itWaitOutside = std::find_if(externalCbWaitEventContainer.begin(),
                                      externalCbWaitEventContainer.end(),
                                      [eventHandle](const ExternalWaitCbEventsInfo &info) {
                                          return std::find(info.waitEvents.begin(), info.waitEvents.end(), eventHandle) != info.waitEvents.end();
                                      });
    ASSERT_NE(externalCbWaitEventContainer.end(), itWaitOutside);

    graph.reset();
    zeEventDestroy(eventHandle);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            givenInOrderCmdListAndRegularCbEventRecordedWithApiGraphExternalFlagWhenInstantiateToGraphAndRunningOnOutsideImmediateCmdListThenRecordAsExternalCbEventAndDispatchCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    alignas(4) uint8_t noopSemWait[sizeof(MI_SEMAPHORE_WAIT)] = {};
    alignas(4) uint8_t noopLri[sizeof(MI_LOAD_REGISTER_IMM)] = {};

    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    auto event = L0::Event::fromHandle(eventHandle);

    ze_event_exp_flags_desc_t eventFlagsDesc = {ZE_STRUCTURE_TYPE_EVENT_EXP_FLAGS_DESC, nullptr, ZE_EVENT_EXP_FLAG_APPEND_GRAPH_EXTERNAL};

    // now record wait and signal on event with external flag
    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEventsWithParameters(commandListHandle, &eventFlagsDesc, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(commandListHandle, &eventFlagsDesc, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));
    ExecutableGraph execGraph;
    auto ret = execGraph.instantiateFrom(*(srcGraph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    auto execGraphHandle = execGraph.toHandle();

    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    ASSERT_EQ(1u, externalCbSignalEventContainer.size());

    auto itSignal = std::find_if(externalCbSignalEventContainer.begin(),
                                 externalCbSignalEventContainer.end(),
                                 [event](const ExternalSignalCbEventInfo &info) { return info.event == event; });
    ASSERT_NE(externalCbSignalEventContainer.end(), itSignal);
    EXPECT_TRUE((*itSignal).apiRequiredSignalEvent);

    const auto &externalCbWaitEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbWaitEventInfos();
    ASSERT_EQ(1u, externalCbWaitEventContainer.size());
    const auto &info = externalCbWaitEventContainer[0];
    ASSERT_EQ(1u, info.waitEvents.size());
    EXPECT_EQ(eventHandle, info.waitEvents[0]);

    auto mclBackendCmdList = externalCbWaitEventContainer[0].executor->asMutable();
    ASSERT_NE(nullptr, mclBackendCmdList);
    auto whiteBoxMcl = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(mclBackendCmdList);
    ASSERT_NE(0u, whiteBoxMcl->eventMutations.size());
    auto &eventMutationPoint = whiteBoxMcl->eventMutations[0];
    ASSERT_EQ(1u, eventMutationPoint.waitEvents.size());
    auto waitExternalVariable = eventMutationPoint.waitEvents[0].eventVariable;
    EXPECT_TRUE(waitExternalVariable->getDesc().eventValue.apiRequiredExternal);
    EXPECT_TRUE(waitExternalVariable->getDesc().eventValue.isExternalFlag);
    ASSERT_EQ(2u, waitExternalVariable->getSemWaitList().size());

    // first sem wait is for patch preamble, second is for in order counter
    auto mutableSemWaitPatchPreamble = waitExternalVariable->getSemWaitList()[0];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWaitPatchPreambleCounter, mutableSemWaitPatchPreamble->getType());
    auto mutableSemWaitInOrderCounter = waitExternalVariable->getSemWaitList()[1];
    EXPECT_EQ(L0::MCL::MutableSemaphoreWait::cbEventWait, mutableSemWaitInOrderCounter->getType());

    // now execute immediate command list and signal the event, but since it is a immediate, no patch preamble, but in order counter is still used
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(commandListHandle, eventHandle));

    auto outsidePatchPreambleCounter = event->getInOrderExecEventHelper().getPatchPreambleCounter();
    auto outsidePatchPreambleDeviceGpuAddress = event->getInOrderExecEventHelper().getPatchPreambleDeviceGpuAddress();
    EXPECT_EQ(0u, outsidePatchPreambleCounter);
    EXPECT_EQ(0u, outsidePatchPreambleDeviceGpuAddress);
    auto outsideInOrderCounter = event->getInOrderExecBaseSignalValue();
    auto outsideInOrderCounterGpuAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();

    // now use it on target exec graph
    ret = L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    // graph uses command views to program and mutate commands, noop patch preamble and program wait on in order counter
    EXPECT_EQ(0, memcmp(mutableSemWaitPatchPreamble->getCommandView(), noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));

    auto semWaitCmdInOrderCounter = genCmdCast<MI_SEMAPHORE_WAIT *>(mutableSemWaitInOrderCounter->getCommandView());
    ASSERT_NE(nullptr, semWaitCmdInOrderCounter);
    EXPECT_EQ(outsideInOrderCounterGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdInOrderCounter));

    if (waitExternalVariable->getDesc().eventValue.qwordIndirect) {
        // qword indirect means counter value is at lri commands, 1st pair is for patch preamble (1st is lower), 2nd pair is for in order counter (3rd is lower)
        ASSERT_EQ(4u, waitExternalVariable->getLoadRegImmList().size());
        auto mutableLriPatchPreambleLower = waitExternalVariable->getLoadRegImmList()[0];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter, mutableLriPatchPreambleLower->getType());
        EXPECT_EQ(0, memcmp(mutableLriPatchPreambleLower->getCommandView(), noopLri, sizeof(MI_LOAD_REGISTER_IMM)));

        auto mutableLriInOrderCounterLower = waitExternalVariable->getLoadRegImmList()[2];
        EXPECT_EQ(L0::MCL::MutableLoadRegisterImm::cbEventWaitLoadCounter, mutableLriInOrderCounterLower->getType());
        auto lriCmdInOrderCounterLower = genCmdCast<MI_LOAD_REGISTER_IMM *>(mutableLriInOrderCounterLower->getCommandView());
        ASSERT_NE(nullptr, lriCmdInOrderCounterLower);
        EXPECT_EQ(outsideInOrderCounter, lriCmdInOrderCounterLower->getDataDword());
    } else {
        EXPECT_EQ(outsideInOrderCounter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmdInOrderCounter));
    }

    // and now since the event is used on the target exec graph, as a signal event, it should have its own patch preamble, which should have non-zero counter and address
    auto execGraphPatchPreambleCounter = event->getInOrderExecEventHelper().getPatchPreambleCounter();
    auto execGraphPatchPreambleDeviceGpuAddress = event->getInOrderExecEventHelper().getPatchPreambleDeviceGpuAddress();
    EXPECT_NE(0u, execGraphPatchPreambleDeviceGpuAddress);
    EXPECT_NE(0u, execGraphPatchPreambleCounter);
    // verify that attaching the event as signal set correctly api required external graph flag
    EXPECT_TRUE(event->getApiRequiredGraphExternalEvent());

    srcGraph.reset();
    zeEventDestroy(eventHandle);
}

struct ForkedGraphWithExternalSignalEventsTest {
    template <typename FamilyType>
    void executeGraph(L0::Device *device, L0::Context *context) {
        ze_result_t returnValue;
        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
        root.reset(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
        root->setOrdinal(0);
        rootQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(root.get())->cmdQImmediate);
        fork.reset(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
        fork->setOrdinal(0);
        forkQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(fork.get())->cmdQImmediate);

        ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &internalEvent));
        eventDesc.flags |= ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &signalFromRootExternalEvent));
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &signalFromForkExternalEvent));

        srcGraph = std::make_unique<L0::Graph>(context, true, false);
        ze_graph_handle_t graphHandle = srcGraph->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(root->toHandle(), graphHandle, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(root->toHandle(), internalEvent, 0U, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(fork->toHandle(), signalFromForkExternalEvent, 1U, &internalEvent));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(fork->toHandle(), internalEvent, 0U, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(root->toHandle(), nullptr, 1U, &internalEvent));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(root->toHandle(), signalFromRootExternalEvent, 0U, nullptr));
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(root->toHandle(), &graphHandle, nullptr));
        srcGraph->enablePatchPreambleCrossSync();

        GraphInstatiateSettings settings{};
        settings.forkPolicy = GraphInstatiateSettings::ForkPolicy::ForkPolicyMonolythicLevels;
        ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph, settings));
        EXPECT_EQ(2u, execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos().size());

        auto rootStream = root->getCmdContainer().getCommandStream();
        auto forkStream = fork->getCmdContainer().getCommandStream();
        auto rootUsedBefore = rootStream->getUsed();
        auto forkUsedBefore = forkStream->getUsed();
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(root->toHandle(), execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(rootCommands, ptrOffset(rootStream->getCpuBase(), rootUsedBefore), rootStream->getUsed() - rootUsedBefore));
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(forkCommands, ptrOffset(forkStream->getCpuBase(), forkUsedBefore), forkStream->getUsed() - forkUsedBefore));
    }

    ~ForkedGraphWithExternalSignalEventsTest() {
        srcGraph.reset();
        zeEventDestroy(internalEvent);
        zeEventDestroy(signalFromForkExternalEvent);
        zeEventDestroy(signalFromRootExternalEvent);
    }

    void verifyPatchPreambleCountersAreUsedByExternalEvents() {
        auto &rootEventHelper = L0::Event::fromHandle(signalFromRootExternalEvent)->getInOrderExecEventHelper();
        EXPECT_EQ(rootQueue->patchPreambleCounter.counter, rootEventHelper.getPatchPreambleCounter());
        EXPECT_EQ(rootQueue->patchPreambleCounter.deviceNodeGpuAddress, rootEventHelper.getPatchPreambleDeviceGpuAddress());
        auto &forkEventHelper = L0::Event::fromHandle(signalFromForkExternalEvent)->getInOrderExecEventHelper();
        EXPECT_EQ(forkQueue->patchPreambleCounter.counter, forkEventHelper.getPatchPreambleCounter());
        EXPECT_EQ(forkQueue->patchPreambleCounter.deviceNodeGpuAddress, forkEventHelper.getPatchPreambleDeviceGpuAddress());
    }

    template <typename FamilyType>
    static std::vector<uint64_t> getSemaphoreWaitAddresses(GenCmdList &commands) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        std::vector<uint64_t> addresses;
        for (auto &semWaitIt : findAll<MI_SEMAPHORE_WAIT *>(commands.begin(), commands.end())) {
            addresses.push_back(NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(genCmdCast<MI_SEMAPHORE_WAIT *>(*semWaitIt)));
        }
        return addresses;
    }

    std::unique_ptr<L0::CommandList> root;
    std::unique_ptr<L0::CommandList> fork;
    CommandQueue *rootQueue = nullptr;
    CommandQueue *forkQueue = nullptr;
    ze_event_handle_t internalEvent = nullptr;
    ze_event_handle_t signalFromRootExternalEvent = nullptr;
    ze_event_handle_t signalFromForkExternalEvent = nullptr;
    std::unique_ptr<L0::Graph> srcGraph;
    MockExecutableGraph execGraph;
    GenCmdList rootCommands;
    GenCmdList forkCommands;
};

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenInOrderCmdListsAndMonolithicPolicyOnAForkedGraphWhenExternalSignalEventsUsedThenPatchPreambleCrossSyncsUseSamePatchPreambleCountersAsExternalEvents) {
    debugManager.flags.OverrideGraphPatchPreambleBarrierSupport.set(0);
    GraphsCleanupGuard graphCleanup;
    ForkedGraphWithExternalSignalEventsTest graph;
    graph.executeGraph<FamilyType>(device, context);
    graph.verifyPatchPreambleCountersAreUsedByExternalEvents();

    EXPECT_EQ(std::vector<uint64_t>{graph.forkQueue->patchPreambleCounter.deviceNodeGpuAddress},
              ForkedGraphWithExternalSignalEventsTest::getSemaphoreWaitAddresses<FamilyType>(graph.rootCommands));
    EXPECT_EQ(std::vector<uint64_t>{graph.rootQueue->patchPreambleCounter.deviceNodeGpuAddress},
              ForkedGraphWithExternalSignalEventsTest::getSemaphoreWaitAddresses<FamilyType>(graph.forkCommands));
}

HWTEST2_F(GraphPatchPreambleBarrierTest,
          GivenInOrderCmdListsAndMonolithicPolicyOnAForkedGraphWhenExternalSignalEventsUsedThenPatchPreamblesUseSamePatchPreambleCountersAsExternalEventsAndAreSynchronizedByBarrier,
          IsAtLeastXe3pCore) {
    GraphsCleanupGuard graphCleanup;
    ForkedGraphWithExternalSignalEventsTest graph;
    graph.executeGraph<FamilyType>(device, context);
    graph.verifyPatchPreambleCountersAreUsedByExternalEvents();

    auto barrierCounterGpuAddress = graph.execGraph.patchPreambleBarrierCounter.counterGpuAddress;
    EXPECT_NE(0u, barrierCounterGpuAddress);
    const std::vector<uint64_t> barrierArriveAndRelease(2, barrierCounterGpuAddress);
    EXPECT_EQ(barrierArriveAndRelease, ForkedGraphWithExternalSignalEventsTest::getSemaphoreWaitAddresses<FamilyType>(graph.rootCommands));
    EXPECT_EQ(barrierArriveAndRelease, ForkedGraphWithExternalSignalEventsTest::getSemaphoreWaitAddresses<FamilyType>(graph.forkCommands));
}

struct ForkedGraphForBarrierTest {
    ForkedGraphForBarrierTest(L0::Device *device, L0::Context *context, uint32_t numForks) {
        ze_result_t returnValue;
        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
        root.reset(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
        root->setOrdinal(0);
        for (uint32_t i = 0; i < numForks; i++) {
            auto &fork = forks.emplace_back(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
            fork->setOrdinal(0);
        }

        ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &forkEvent));
        joinEvents.resize(numForks);
        for (auto &joinEvent : joinEvents) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &joinEvent));
        }

        srcGraph = std::make_unique<L0::Graph>(context, true, false);
        ze_graph_handle_t graphHandle = srcGraph->toHandle();
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(root->toHandle(), graphHandle, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(root->toHandle(), forkEvent, 0U, nullptr));
        for (uint32_t i = 0; i < numForks; i++) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(forks[i]->toHandle(), nullptr, 1U, &forkEvent));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(forks[i]->toHandle(), joinEvents[i], 0U, nullptr));
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(root->toHandle(), nullptr, numForks, joinEvents.data()));
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(root->toHandle(), &graphHandle, nullptr));
        srcGraph->enablePatchPreambleCrossSync();

        GraphInstatiateSettings settings{};
        settings.forkPolicy = GraphInstatiateSettings::ForkPolicy::ForkPolicyMonolythicLevels;
        EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph, settings));
        EXPECT_EQ(numForks + 1, execGraph.getOrderedCommands()->size());
    }

    ~ForkedGraphForBarrierTest() {
        srcGraph.reset();
        zeEventDestroy(forkEvent);
        for (auto &joinEvent : joinEvents) {
            zeEventDestroy(joinEvent);
        }
    }

    uint32_t getBarrierWeight() const {
        uint32_t weight = root->getImmediateQueuePartitionCount();
        for (auto &fork : forks) {
            weight += fork->getImmediateQueuePartitionCount();
        }
        return weight;
    }

    std::unique_ptr<L0::CommandList> root;
    std::vector<std::unique_ptr<L0::CommandList>> forks;
    ze_event_handle_t forkEvent = nullptr;
    std::vector<ze_event_handle_t> joinEvents;
    std::unique_ptr<L0::Graph> srcGraph;
    MockExecutableGraph execGraph;
};

template <typename FamilyType>
struct GraphBarrierStreamCommands {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    GraphBarrierStreamCommands(L0::CommandList &cmdList, size_t usedBefore, bool qwordIndirect) : qwordIndirect(qwordIndirect) {
        auto stream = cmdList.getCmdContainer().getCommandStream();
        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(parsedCommands, ptrOffset(stream->getCpuBase(), usedBefore), stream->getUsed() - usedBefore));
        arbChecks = findAll<MI_ARB_CHECK *>(parsedCommands.begin(), parsedCommands.end());
        atomics = findAll<MI_ATOMIC *>(parsedCommands.begin(), parsedCommands.end());
        EXPECT_EQ(2u, arbChecks.size());
        if (arbChecks.size() == 2u) {
            semWaitsBeforePreamble = findAll<MI_SEMAPHORE_WAIT *>(parsedCommands.begin(), arbChecks[0]);
            auto bbStartIt = find<MI_BATCH_BUFFER_START *>(arbChecks[1], parsedCommands.end());
            semWaitsAfterPreamble = findAll<MI_SEMAPHORE_WAIT *>(arbChecks[1], bbStartIt);
        }
    }

    uint64_t getAddress(GenCmdList::iterator semWaitIt) {
        return NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(genCmdCast<MI_SEMAPHORE_WAIT *>(*semWaitIt));
    }

    uint64_t getValue(GenCmdList::iterator semWaitIt) {
        if (qwordIndirect) {
            auto lriCmds = findAll<MI_LOAD_REGISTER_IMM *>(parsedCommands.begin(), semWaitIt);
            if (lriCmds.size() < 2u) {
                ADD_FAILURE();
                return 0;
            }
            auto lriLower = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lriCmds[lriCmds.size() - 2]);
            auto lriHigher = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lriCmds[lriCmds.size() - 1]);
            return (static_cast<uint64_t>(lriHigher->getDataDword()) << 32) | lriLower->getDataDword();
        }
        return NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(genCmdCast<MI_SEMAPHORE_WAIT *>(*semWaitIt));
    }

    void verifyBarrier(uint64_t barrierAddress, uint64_t arriveValue, uint64_t releaseValue) {
        // in order counters might be signaled with atomics too, count only barrier ones
        size_t barrierAtomics = 0;
        for (auto &atomicIt : atomics) {
            if (barrierAddress == genCmdCast<MI_ATOMIC *>(*atomicIt)->getMemoryAddress()) {
                barrierAtomics++;
            }
        }
        EXPECT_EQ(2u, barrierAtomics);
        ASSERT_NE(0u, semWaitsBeforePreamble.size());
        auto arriveIt = semWaitsBeforePreamble.back();
        EXPECT_EQ(barrierAddress, getAddress(arriveIt));
        EXPECT_EQ(arriveValue, getValue(arriveIt));
        ASSERT_EQ(1u, semWaitsAfterPreamble.size());
        EXPECT_EQ(barrierAddress, getAddress(semWaitsAfterPreamble[0]));
        EXPECT_EQ(releaseValue, getValue(semWaitsAfterPreamble[0]));
    }

    GenCmdList parsedCommands;
    std::vector<GenCmdList::iterator> arbChecks;
    std::vector<GenCmdList::iterator> atomics;
    std::vector<GenCmdList::iterator> semWaitsBeforePreamble;
    std::vector<GenCmdList::iterator> semWaitsAfterPreamble;
    bool qwordIndirect = false;
};

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenInOrderCmdListsAndMonolithicPolicyOnAForkedGraphWhenGraphIsExecutedMoreThanOnceThenInOrderCompletionIsUsedToSynchronizeBeforePatchPreamble) {
    debugManager.flags.OverrideGraphPatchPreambleBarrierSupport.set(0);
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    GraphsCleanupGuard graphCleanup;

    const bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, neoDevice->getDeviceInfo().semaphore64bCmdSupport);

    ForkedGraphForBarrierTest graph(device, context, 1);
    std::array<L0::CommandList *, 2> executors = {graph.root.get(), graph.forks[0].get()};

    for (size_t execution = 0; execution < 2; execution++) {
        std::array<size_t, 2> usedBefore;
        std::array<uint64_t, 2> countersBefore;
        for (size_t i = 0; i < executors.size(); i++) {
            usedBefore[i] = executors[i]->getCmdContainer().getCommandStream()->getUsed();
            countersBefore[i] = executors[i]->getInOrderExecInfo()->getCounterValue();
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));

        for (size_t i = 0; i < executors.size(); i++) {
            GraphBarrierStreamCommands<FamilyType> commands(*executors[i], usedBefore[i], qwordIndirect);
            if (execution == 0) {
                EXPECT_EQ(0u, commands.semWaitsBeforePreamble.size());
                continue;
            }
            auto &otherInOrderExecInfo = executors[1 - i]->getInOrderExecInfo();
            ASSERT_EQ(1u, commands.semWaitsBeforePreamble.size());
            EXPECT_EQ(otherInOrderExecInfo->getBaseDeviceAddress() + otherInOrderExecInfo->getAllocationOffset(), commands.getAddress(commands.semWaitsBeforePreamble[0]));
            EXPECT_EQ(countersBefore[1 - i], commands.getValue(commands.semWaitsBeforePreamble[0]));
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenInOrderCmdListsAndMonolithicPolicyOnAForkedGraphWhenImmediateResourcesOfForkExecutorFailToInitializeThenGraphExecutionReturnsError) {
    GraphsCleanupGuard graphCleanup;

    ForkedGraphForBarrierTest graph(device, context, 1);
    CommandList::whiteboxCast(graph.forks[0].get())->immediateResourcesInitializationResult = ZE_RESULT_ERROR_DEVICE_LOST;

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
}

HWTEST2_F(GraphPatchPreambleBarrierTest,
          GivenInOrderCmdListsAndMonolithicPolicyOnAForkedGraphWhenGraphIsExecutedWithBarrierThenPatchPreambleCountersOfExecutorsAreNotAcquired,
          IsAtLeastXe3pCore) {
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    GraphsCleanupGuard graphCleanup;

    ForkedGraphForBarrierTest graph(device, context, 1);
    auto rootQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(graph.root.get())->cmdQImmediate);

    for (uint32_t execution = 0; execution < 2; execution++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
        EXPECT_TRUE(graph.execGraph.segmentsCrossSync.useBarrier);
        EXPECT_NE(nullptr, graph.execGraph.segmentsCrossSync.barrier.counterAllocation);
        EXPECT_TRUE(graph.execGraph.graphWidePatchPreambleCrossSync.empty());

        auto forkQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(graph.forks[0].get())->cmdQImmediate);
        EXPECT_EQ(0u, rootQueue->patchPreambleCounter.counter);
        EXPECT_EQ(0u, forkQueue->patchPreambleCounter.counter);
    }
}

HWTEST2_F(GraphPatchPreambleBarrierTest,
          GivenInOrderCmdListsAndMonolithicPolicyOnAForkedGraphWhenGraphIsExecutedMoreThanOnceThenBarrierIsUsedToSynchronizeBeforeAndAfterPatchPreamble,
          IsAtLeastXe3pCore) {
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    GraphsCleanupGuard graphCleanup;

    const bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, neoDevice->getDeviceInfo().semaphore64bCmdSupport);

    ForkedGraphForBarrierTest graph(device, context, 1);
    const uint64_t weight = graph.getBarrierWeight();
    std::array<L0::CommandList *, 2> executors = {graph.root.get(), graph.forks[0].get()};

    for (uint64_t execution = 1; execution <= 3; execution++) {
        std::array<size_t, 2> usedBefore;
        for (size_t i = 0; i < executors.size(); i++) {
            usedBefore[i] = executors[i]->getCmdContainer().getCommandStream()->getUsed();
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));

        auto barrierAddress = graph.execGraph.patchPreambleBarrierCounter.counterGpuAddress;
        EXPECT_NE(0u, barrierAddress);
        EXPECT_EQ(2 * execution * weight, graph.execGraph.patchPreambleBarrierCounter.counterValue);
        for (size_t i = 0; i < executors.size(); i++) {
            GraphBarrierStreamCommands<FamilyType> commands(*executors[i], usedBefore[i], qwordIndirect);
            // only barrier arrive before the patch preamble - executors do not wait for each other in order counters anymore
            EXPECT_EQ(1u, commands.semWaitsBeforePreamble.size());
            commands.verifyBarrier(barrierAddress, (2 * execution - 1) * weight, 2 * execution * weight);
        }
    }
}

HWTEST2_F(GraphPatchPreambleBarrierTest,
          GivenInOrderCmdListsAndMonolithicPolicyOnAGraphWithMultipleForksWhenGraphIsExecutedThenEachExecutorDispatchesSingleBarrierArriveAndRelease,
          IsAtLeastXe3pCore) {
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    GraphsCleanupGuard graphCleanup;

    const bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, neoDevice->getDeviceInfo().semaphore64bCmdSupport);

    constexpr uint32_t numForks = 4;
    ForkedGraphForBarrierTest graph(device, context, numForks);
    const uint64_t weight = graph.getBarrierWeight();
    std::vector<L0::CommandList *> executors = {graph.root.get()};
    for (auto &fork : graph.forks) {
        executors.push_back(fork.get());
    }

    for (uint64_t execution = 1; execution <= 2; execution++) {
        std::vector<size_t> usedBefore;
        for (auto executor : executors) {
            usedBefore.push_back(executor->getCmdContainer().getCommandStream()->getUsed());
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));

        auto barrierAddress = graph.execGraph.patchPreambleBarrierCounter.counterGpuAddress;
        for (size_t i = 0; i < executors.size(); i++) {
            GraphBarrierStreamCommands<FamilyType> commands(*executors[i], usedBefore[i], qwordIndirect);
            EXPECT_EQ(1u, commands.semWaitsBeforePreamble.size());
            commands.verifyBarrier(barrierAddress, (2 * execution - 1) * weight, 2 * execution * weight);
        }
    }
}

HWTEST2_F(GraphPatchPreambleBarrierTest,
          GivenForkedGraphWhenSubmissionOfFirstSegmentFailsThenBarrierCounterIsNotAdvancedAndNextExecutionUsesSameBarrierValues,
          IsAtLeastXe3pCore) {
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    GraphsCleanupGuard graphCleanup;

    const bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, neoDevice->getDeviceInfo().semaphore64bCmdSupport);

    ForkedGraphForBarrierTest graph(device, context, 1);
    const uint64_t weight = graph.getBarrierWeight();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, graph.execGraph.execute(graph.root.get(), nullptr, nullptr, 1, nullptr));
    EXPECT_TRUE(graph.execGraph.segmentsCrossSync.useBarrier);
    EXPECT_EQ(0u, graph.execGraph.patchPreambleBarrierCounter.counterValue);

    std::array<L0::CommandList *, 2> executors = {graph.root.get(), graph.forks[0].get()};
    std::array<size_t, 2> usedBefore;
    for (size_t i = 0; i < executors.size(); i++) {
        usedBefore[i] = executors[i]->getCmdContainer().getCommandStream()->getUsed();
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(2 * weight, graph.execGraph.patchPreambleBarrierCounter.counterValue);

    auto barrierAddress = graph.execGraph.patchPreambleBarrierCounter.counterGpuAddress;
    for (size_t i = 0; i < executors.size(); i++) {
        GraphBarrierStreamCommands<FamilyType> commands(*executors[i], usedBefore[i], qwordIndirect);
        commands.verifyBarrier(barrierAddress, weight, 2 * weight);
    }
}

HWTEST2_F(GraphPatchPreambleBarrierTest,
          GivenForkedGraphExecutedOnRootCommandListWhenGraphIsExecutedOnAnotherRootCommandListThenPreviousRootSubmissionOnOtherCsrIsAwaitedBeforeBarrierArrive,
          IsAtLeastXe3pCore) {
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    GraphsCleanupGuard graphCleanup;

    const bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, neoDevice->getDeviceInfo().semaphore64bCmdSupport);

    ForkedGraphForBarrierTest graph(device, context, 1);
    const uint64_t weight = graph.getBarrierWeight();

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
    auto rootQueue = CommandList::whiteboxCast(graph.root.get())->cmdQImmediate;
    auto rootTagGpuAddress = rootQueue->getCsr()->getTagAllocation()->getGpuAddress();
    auto rootTaskCountAfterFirst = rootQueue->getTaskCount();

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> secondRoot(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    secondRoot->setOrdinal(0);
    const bool previousRootSubmittedOnOtherCsr = rootQueue->getCsr() != CommandList::whiteboxCast(secondRoot.get())->cmdQImmediate->getCsr();
    ASSERT_EQ(weight, secondRoot->getImmediateQueuePartitionCount() + graph.forks[0]->getImmediateQueuePartitionCount());

    auto secondRootUsedBefore = secondRoot->getCmdContainer().getCommandStream()->getUsed();
    auto forkUsedBefore = graph.forks[0]->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(secondRoot->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));

    auto barrierAddress = graph.execGraph.patchPreambleBarrierCounter.counterGpuAddress;

    GraphBarrierStreamCommands<FamilyType> secondRootCommands(*secondRoot, secondRootUsedBefore, qwordIndirect);
    if (previousRootSubmittedOnOtherCsr) {
        ASSERT_EQ(2u, secondRootCommands.semWaitsBeforePreamble.size());
        EXPECT_EQ(rootTagGpuAddress, secondRootCommands.getAddress(secondRootCommands.semWaitsBeforePreamble[0]));
        EXPECT_EQ(rootTaskCountAfterFirst, secondRootCommands.getValue(secondRootCommands.semWaitsBeforePreamble[0]));
    } else {
        EXPECT_EQ(1u, secondRootCommands.semWaitsBeforePreamble.size());
    }
    secondRootCommands.verifyBarrier(barrierAddress, 3 * weight, 4 * weight);

    GraphBarrierStreamCommands<FamilyType> forkCommands(*graph.forks[0], forkUsedBefore, qwordIndirect);
    EXPECT_EQ(1u, forkCommands.semWaitsBeforePreamble.size());
    forkCommands.verifyBarrier(barrierAddress, 3 * weight, 4 * weight);

    secondRootUsedBefore = secondRoot->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(secondRoot->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
    GraphBarrierStreamCommands<FamilyType> secondRootNextCommands(*secondRoot, secondRootUsedBefore, qwordIndirect);
    EXPECT_EQ(1u, secondRootNextCommands.semWaitsBeforePreamble.size());
    secondRootNextCommands.verifyBarrier(barrierAddress, 5 * weight, 6 * weight);
}

HWTEST2_F(GraphTestInstantiationTest,
          GivenGraphPatchPreambleBarrierSupportedWhenQueueDoesNotTrackSubmissionsForPatchPreambleThenPatchPreambleCountersAreAwaitedInsteadOfBarrier,
          IsAtLeastXe3pCore) {
    debugManager.flags.OverrideGraphPatchPreambleBarrierSupport.set(1);
    debugManager.flags.ContextGroupSize.set(0);
    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    GraphsCleanupGuard graphCleanup;

    const bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(FamilyType::isQwordInOrderCounter, neoDevice->getDeviceInfo().semaphore64bCmdSupport);

    ForkedGraphForBarrierTest graph(device, context, 1);
    std::array<L0::CommandList *, 2> executors = {graph.root.get(), graph.forks[0].get()};
    std::array<size_t, 2> usedBefore;
    for (size_t i = 0; i < executors.size(); i++) {
        ASSERT_FALSE(executors[i]->isImmediateQueueWaitForPreambleSaved());
        usedBefore[i] = executors[i]->getCmdContainer().getCommandStream()->getUsed();
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));

    EXPECT_EQ(nullptr, graph.execGraph.patchPreambleBarrierCounter.counterNode);
    for (size_t i = 0; i < executors.size(); i++) {
        auto otherQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(executors[1 - i])->cmdQImmediate);
        GraphBarrierStreamCommands<FamilyType> commands(*executors[i], usedBefore[i], qwordIndirect);
        ASSERT_EQ(1u, commands.semWaitsAfterPreamble.size());
        EXPECT_EQ(otherQueue->patchPreambleCounter.deviceNodeGpuAddress, commands.getAddress(commands.semWaitsAfterPreamble[0]));
    }
}

HWTEST2_F(GraphPatchPreambleBarrierTest,
          GivenForkedGraphWhenGraphPatchPreambleIsDisabledThenBarrierIsNotUsed,
          IsAtLeastXe3pCore) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    debugManager.flags.ForceDisableGraphPatchPreamble.set(1);
    GraphsCleanupGuard graphCleanup;

    ForkedGraphForBarrierTest graph(device, context, 1);
    std::array<L0::CommandList *, 2> executors = {graph.root.get(), graph.forks[0].get()};
    std::array<size_t, 2> usedBefore;
    for (size_t i = 0; i < executors.size(); i++) {
        usedBefore[i] = executors[i]->getCmdContainer().getCommandStream()->getUsed();
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(graph.root->toHandle(), graph.execGraph.toHandle(), nullptr, nullptr, 0, nullptr));

    EXPECT_EQ(nullptr, graph.execGraph.patchPreambleBarrierCounter.counterNode);
    for (size_t i = 0; i < executors.size(); i++) {
        auto stream = executors[i]->getCmdContainer().getCommandStream();
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(stream->getCpuBase(), usedBefore[i]), stream->getUsed() - usedBefore[i]));
        for (auto &atomicIt : findAll<MI_ATOMIC *>(cmdList.begin(), cmdList.end())) {
            EXPECT_NE(MI_ATOMIC::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_INC8B, genCmdCast<MI_ATOMIC *>(*atomicIt)->getAtomicOpcode());
        }
    }
}

TEST_F(GraphTestInstantiationTest, givenGraphPatchPreambleBarrierCounterWhenPreparingExecutionsThenSingleCounterIsUsedAndBarrierValuesExceed32Bits) {
    GraphPatchPreambleBarrierCounter barrierCounter;
    PatchPreambleBarrier barrier;
    constexpr uint32_t weight = 3;

    barrierCounter.prepareExecution(*device, weight, barrier);
    ASSERT_NE(nullptr, barrierCounter.counterNode);
    ASSERT_NE(nullptr, barrier.counterAllocation);
    auto counterAddress = barrierCounter.counterNode->getGpuAddress();
    EXPECT_EQ(counterAddress, barrierCounter.counterGpuAddress);
    EXPECT_EQ(0u, *reinterpret_cast<uint64_t *>(barrierCounter.counterNode->getCpuBase()));

    EXPECT_EQ(counterAddress, barrier.counterGpuAddress);
    EXPECT_EQ(weight, barrier.arriveValue);
    EXPECT_EQ(2 * weight, barrier.releaseValue);
    barrierCounter.commitExecution(barrier);

    barrierCounter.prepareExecution(*device, weight, barrier);
    EXPECT_EQ(counterAddress, barrier.counterGpuAddress);
    EXPECT_EQ(3 * weight, barrier.arriveValue);
    EXPECT_EQ(4 * weight, barrier.releaseValue);
    barrierCounter.commitExecution(barrier);

    constexpr uint64_t maxDwordValue = std::numeric_limits<uint32_t>::max();
    barrierCounter.counterValue = maxDwordValue - weight;
    barrierCounter.prepareExecution(*device, weight, barrier);
    EXPECT_EQ(counterAddress, barrier.counterGpuAddress);
    EXPECT_EQ(maxDwordValue, barrier.arriveValue);
    EXPECT_EQ(maxDwordValue + weight, barrier.releaseValue);
    barrierCounter.commitExecution(barrier);
    EXPECT_EQ(maxDwordValue + weight, barrierCounter.counterValue);

    EXPECT_ANY_THROW(barrierCounter.prepareExecution(*device, 0, barrier));
}

TEST_F(GraphTestInstantiationTest, givenGraphPatchPreambleBarrierCounterWhenExecutionIsPreparedButNotCommittedThenNextExecutionGetsSameBarrierValues) {
    GraphPatchPreambleBarrierCounter barrierCounter;
    PatchPreambleBarrier barrier;
    constexpr uint32_t weight = 3;

    barrierCounter.prepareExecution(*device, weight, barrier);
    EXPECT_EQ(0u, barrierCounter.counterValue);

    PatchPreambleBarrier retriedBarrier;
    barrierCounter.prepareExecution(*device, weight, retriedBarrier);
    EXPECT_EQ(barrier.counterGpuAddress, retriedBarrier.counterGpuAddress);
    EXPECT_EQ(weight, retriedBarrier.arriveValue);
    EXPECT_EQ(2 * weight, retriedBarrier.releaseValue);
}

HWTEST_F(GraphTestInstantiationTest, givenTbxCsrWhenPreparingGraphPatchPreambleBarrierThenCounterResetIsUploadedToSimulationOnlyWhenCounterIsAcquired) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.commandStreamReceiverType = NEO::CommandStreamReceiverType::tbx;
    ultCsr.chunkCopySupported = true;
    device->getDeviceInOrderCounterAllocator();
    ultCsr.writeMemoryParams.chunkWrites.clear();

    GraphPatchPreambleBarrierCounter barrierCounter;
    PatchPreambleBarrier barrier;
    constexpr uint32_t weight = 2;

    barrierCounter.prepareExecution(*device, weight, barrier);
    ASSERT_EQ(1u, ultCsr.writeMemoryParams.chunkWrites.size());
    const NEO::WrittenMemoryChunk expectedCounterReset{barrier.counterAllocation,
                                                       ptrDiff(barrierCounter.counterNode->getCpuBase(), barrier.counterAllocation->getUnderlyingBuffer()),
                                                       sizeof(uint64_t)};
    EXPECT_EQ(expectedCounterReset, ultCsr.writeMemoryParams.chunkWrites[0]);
    barrierCounter.commitExecution(barrier);

    ultCsr.writeMemoryParams.chunkWrites.clear();
    barrierCounter.prepareExecution(*device, weight, barrier);
    EXPECT_TRUE(ultCsr.writeMemoryParams.chunkWrites.empty());

    ultCsr.commandStreamReceiverType = NEO::CommandStreamReceiverType::hardware;
}

HWTEST_F(GraphTestInstantiationTest, givenHardwareCsrWhenPreparingGraphPatchPreambleBarrierThenCounterResetIsNotUploadedToSimulation) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.chunkCopySupported = true;
    device->getDeviceInOrderCounterAllocator();
    const auto writeMemoryCallCount = ultCsr.writeMemoryParams.totalCallCount;

    GraphPatchPreambleBarrierCounter barrierCounter;
    PatchPreambleBarrier barrier;
    barrierCounter.prepareExecution(*device, 2u, barrier);

    EXPECT_EQ(0u, *reinterpret_cast<uint64_t *>(barrierCounter.counterNode->getCpuBase()));
    EXPECT_EQ(writeMemoryCallCount, ultCsr.writeMemoryParams.totalCallCount);
}

TEST_F(GraphTestInstantiationTest, givenInOrderCmdListAndRegularCbEventRecordedWithoutApiGraphExternalFlagWhenInstantiateToGraphThenDoNotRecordAsExternalCbEvent) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEventsWithParameters(commandListHandle, nullptr, 1, &eventHandle));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, nullptr, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(commandListHandle, nullptr, eventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    auto event = L0::Event::fromHandle(eventHandle);
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    ASSERT_EQ(0u, externalCbSignalEventContainer.size());

    const auto &externalCbWaitEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbWaitEventInfos();
    ASSERT_EQ(0u, externalCbWaitEventContainer.size());

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphExecution, GivenEmptyExecutableGraphWhenSubmittingItToCommandListThenAppendBarrierHandlingEvents) {
    GraphsCleanupGuard graphCleanup;
    Mock<Event> signalEvents[2];
    Mock<Event> waitEvents[3];
    ze_event_handle_t waitEventsList[3] = {waitEvents[0].toHandle(), waitEvents[1].toHandle(), waitEvents[2].toHandle()};

    Mock<CommandList> cmdlist;
    ExecutableGraph graph;
    EXPECT_TRUE(graph.empty());

    auto res = graph.execute(&cmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(1u, cmdlist.appendBarrierCalled);

    res = graph.execute(&cmdlist, nullptr, signalEvents[0].toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(2u, cmdlist.appendBarrierCalled);

    res = graph.execute(&cmdlist, nullptr, signalEvents[0].toHandle(), 2, waitEventsList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(3u, cmdlist.appendBarrierCalled);

    res = graph.execute(&cmdlist, nullptr, nullptr, 3, waitEventsList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(4u, cmdlist.appendBarrierCalled);
}

TEST_F(GraphExecution, GivenExecutableGraphWhenSubmittingItToCommandListThenAppendIt) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandQueue> cmdQueue;
    Mock<CommandList> cmdlist;
    cmdlist.cmdQImmediate = &cmdQueue;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 0U, nullptr);
    srcGraph.stopCapturing();
    cmdlist.setGraphCaptureTarget(nullptr);

    srcGraph.captureTargetDesc.hDevice = device->toHandle();

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    auto res = execGraph.execute(&cmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(1U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, cmdlist.appendSignalEventCalled);
    cmdlist.cmdQImmediate = nullptr;
}

TEST_F(GraphExecution, GivenExecutableGraphWithSubGraphsWhenSubmittingItToCommandListSubmitAlsoSubGraphsToRespectiveCommandLists) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<CommandQueue> cmdQueue;
    mainRecordCmdlist.device = this->device;
    mainExecCmdlist.cmdQImmediate = &cmdQueue;
    mainExecCmdlist.device = this->device;
    subCmdlist.cmdQImmediate = &cmdQueue;
    subCmdlist.device = this->device;
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    mainRecordCmdlist.setGraphCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdlistHandle, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setGraphCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    execMultiGraph.instantiateFrom(srcGraph, settings);

    EXPECT_EQ(0U, mainRecordCmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, mainRecordCmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, mainRecordCmdlist.appendSignalEventCalled);
    EXPECT_EQ(0U, mainExecCmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, mainExecCmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, mainExecCmdlist.appendSignalEventCalled);
    EXPECT_EQ(0U, subCmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, subCmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, subCmdlist.appendSignalEventCalled);
    auto res = execMultiGraph.execute(&mainExecCmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, mainRecordCmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, mainRecordCmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, mainRecordCmdlist.appendSignalEventCalled);
    EXPECT_EQ(1U, mainExecCmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, mainExecCmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, mainExecCmdlist.appendSignalEventCalled);
    EXPECT_EQ(1U, subCmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, subCmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, subCmdlist.appendSignalEventCalled);

    mainExecCmdlist.cmdQImmediate = nullptr;
    subCmdlist.cmdQImmediate = nullptr;
}

TEST_F(GraphExecution, GivenMonolithicGraphWithoutPostJoinCommandsWhenInstantiatedThenNoTrailingSynchronizationIsInserted) {
    GraphsCleanupGuard graphCleanup;

    DriverBackedGraphContextReturningSpecificCmdList ctx(context->getDriverHandle());
    auto *rootExecCmdList = new CapturingInternalExecCmdList;
    auto *childExecCmdList = new CapturingInternalExecCmdList;
    ctx.cmdListsToReturn.push_back(rootExecCmdList);
    ctx.cmdListsToReturn.push_back(childExecCmdList);

    MockGraphCmdListWithContext rootRecordCmdList{&ctx};
    MockGraphCmdListWithContext childRecordCmdList{&ctx};
    rootRecordCmdList.device = this->device;
    childRecordCmdList.device = this->device;

    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    auto forkEventHandle = forkEvent.toHandle();
    auto joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    auto rootRecordCmdListHandle = rootRecordCmdList.toHandle();
    auto childRecordCmdListHandle = childRecordCmdList.toHandle();
    rootRecordCmdList.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(rootRecordCmdList, false);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, forkEventHandle, 0U, nullptr);
    childRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(childRecordCmdListHandle, joinEventHandle, 1U, &forkEventHandle);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, nullptr, 1U, &joinEventHandle);
    srcGraph.stopCapturing();
    rootRecordCmdList.setGraphCaptureTarget(nullptr);

    ExposedExecutableGraph execGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph, settings));

    EXPECT_EQ(nullptr, execGraph.trailingEventsPool);
    EXPECT_EQ(0u, rootExecCmdList->appendWaitOnEventsCalled);
    EXPECT_TRUE(rootExecCmdList->waitedEvents.empty());
    EXPECT_EQ(0u, childExecCmdList->appendSignalEventCalled);
    EXPECT_TRUE(childExecCmdList->signaledEvents.empty());
}

TEST_F(GraphExecution, GivenMonolithicGraphWithSingleForkEventAwaitedByMultipleCommandListsWhenInstantiatedThenNoTrailingSynchronizationIsInserted) {
    GraphsCleanupGuard graphCleanup;

    DriverBackedGraphContextReturningSpecificCmdList ctx(context->getDriverHandle());
    auto *rootExecCmdList = new CapturingInternalExecCmdList;
    auto *firstChildExecCmdList = new CapturingInternalExecCmdList;
    auto *secondChildExecCmdList = new CapturingInternalExecCmdList;
    ctx.cmdListsToReturn.push_back(rootExecCmdList);
    ctx.cmdListsToReturn.push_back(firstChildExecCmdList);
    ctx.cmdListsToReturn.push_back(secondChildExecCmdList);

    MockGraphCmdListWithContext rootRecordCmdList{&ctx};
    MockGraphCmdListWithContext firstChildRecordCmdList{&ctx};
    MockGraphCmdListWithContext secondChildRecordCmdList{&ctx};
    rootRecordCmdList.device = this->device;
    firstChildRecordCmdList.device = this->device;
    secondChildRecordCmdList.device = this->device;

    Mock<Event> forkEvent;
    Mock<Event> firstJoinEvent;
    Mock<Event> secondJoinEvent;
    auto forkEventHandle = forkEvent.toHandle();
    auto firstJoinEventHandle = firstJoinEvent.toHandle();
    auto secondJoinEventHandle = secondJoinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    auto rootRecordCmdListHandle = rootRecordCmdList.toHandle();
    auto firstChildRecordCmdListHandle = firstChildRecordCmdList.toHandle();
    auto secondChildRecordCmdListHandle = secondChildRecordCmdList.toHandle();
    rootRecordCmdList.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(rootRecordCmdList, false);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, forkEventHandle, 0U, nullptr);
    firstChildRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(firstChildRecordCmdListHandle, firstJoinEventHandle, 1U, &forkEventHandle);
    secondChildRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(secondChildRecordCmdListHandle, secondJoinEventHandle, 1U, &forkEventHandle);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, nullptr, 1U, &firstJoinEventHandle);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, nullptr, 1U, &secondJoinEventHandle);
    srcGraph.stopCapturing();
    rootRecordCmdList.setGraphCaptureTarget(nullptr);

    ExposedExecutableGraph execGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph, settings));

    EXPECT_EQ(nullptr, execGraph.trailingEventsPool);
    EXPECT_EQ(0u, rootExecCmdList->appendWaitOnEventsCalled);
    EXPECT_EQ(0u, firstChildExecCmdList->appendSignalEventCalled);
    EXPECT_EQ(0u, secondChildExecCmdList->appendSignalEventCalled);
}

TEST_F(GraphExecution, GivenMonolithicGraphWithPostJoinCommandsWhenInstantiatedThenTrailingSynchronizationIsInserted) {
    GraphsCleanupGuard graphCleanup;

    DriverBackedGraphContextReturningSpecificCmdList ctx(context->getDriverHandle());
    auto *rootExecCmdList = new CapturingInternalExecCmdList;
    auto *childExecCmdList = new CapturingInternalExecCmdList;
    ctx.cmdListsToReturn.push_back(rootExecCmdList);
    ctx.cmdListsToReturn.push_back(childExecCmdList);

    MockGraphCmdListWithContext rootRecordCmdList{&ctx};
    MockGraphCmdListWithContext childRecordCmdList{&ctx};
    rootRecordCmdList.device = this->device;
    childRecordCmdList.device = this->device;

    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    Mock<Event> postJoinSignalEvent;
    auto forkEventHandle = forkEvent.toHandle();
    auto joinEventHandle = joinEvent.toHandle();
    auto postJoinSignalEventHandle = postJoinSignalEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    auto rootRecordCmdListHandle = rootRecordCmdList.toHandle();
    auto childRecordCmdListHandle = childRecordCmdList.toHandle();
    rootRecordCmdList.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(rootRecordCmdList, false);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, forkEventHandle, 0U, nullptr);
    childRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(childRecordCmdListHandle, joinEventHandle, 1U, &forkEventHandle);
    childRecordCmdList.capture<CaptureApi::zeCommandListAppendSignalEvent>(childRecordCmdListHandle, postJoinSignalEventHandle);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, nullptr, 1U, &joinEventHandle);
    srcGraph.stopCapturing();
    rootRecordCmdList.setGraphCaptureTarget(nullptr);

    ExposedExecutableGraph execGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph, settings));

    ASSERT_NE(nullptr, execGraph.trailingEventsPool);
    EXPECT_EQ(2u, childExecCmdList->appendSignalEventCalled);
    ASSERT_EQ(2u, childExecCmdList->signaledEvents.size());
    EXPECT_EQ(postJoinSignalEventHandle, childExecCmdList->signaledEvents[0]);
    EXPECT_NE(postJoinSignalEventHandle, childExecCmdList->signaledEvents[1]);

    EXPECT_EQ(1u, rootExecCmdList->appendWaitOnEventsCalled);
    ASSERT_EQ(1u, rootExecCmdList->waitedEvents.size());
    EXPECT_EQ(childExecCmdList->signaledEvents[1], rootExecCmdList->waitedEvents[0]);
}

TEST_F(GraphExecution, GivenNestedPostJoinSubGraphsWhenInstantiatedMonolithicallyThenRootWaitsForAllTrailingEvents) {
    GraphsCleanupGuard graphCleanup;

    DriverBackedGraphContextReturningSpecificCmdList ctx(context->getDriverHandle());
    auto *rootExecCmdList = new CapturingInternalExecCmdList;
    auto *childExecCmdList = new CapturingInternalExecCmdList;
    auto *grandChildExecCmdList = new CapturingInternalExecCmdList;
    ctx.cmdListsToReturn.push_back(rootExecCmdList);
    ctx.cmdListsToReturn.push_back(childExecCmdList);
    ctx.cmdListsToReturn.push_back(grandChildExecCmdList);

    MockGraphCmdListWithContext rootRecordCmdList{&ctx};
    MockGraphCmdListWithContext childRecordCmdList{&ctx};
    MockGraphCmdListWithContext grandChildRecordCmdList{&ctx};
    rootRecordCmdList.device = this->device;
    childRecordCmdList.device = this->device;
    grandChildRecordCmdList.device = this->device;

    Mock<Event> rootForkEvent;
    Mock<Event> childForkEvent;
    Mock<Event> rootJoinEvent;
    Mock<Event> childJoinEvent;
    Mock<Event> childPostJoinSignalEvent;
    Mock<Event> grandChildPostJoinSignalEvent;
    auto rootForkEventHandle = rootForkEvent.toHandle();
    auto childForkEventHandle = childForkEvent.toHandle();
    auto rootJoinEventHandle = rootJoinEvent.toHandle();
    auto childJoinEventHandle = childJoinEvent.toHandle();
    auto childPostJoinSignalEventHandle = childPostJoinSignalEvent.toHandle();
    auto grandChildPostJoinSignalEventHandle = grandChildPostJoinSignalEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    auto rootRecordCmdListHandle = rootRecordCmdList.toHandle();
    auto childRecordCmdListHandle = childRecordCmdList.toHandle();
    auto grandChildRecordCmdListHandle = grandChildRecordCmdList.toHandle();
    rootRecordCmdList.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(rootRecordCmdList, false);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, rootForkEventHandle, 0U, nullptr);
    childRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(childRecordCmdListHandle, childForkEventHandle, 1U, &rootForkEventHandle);
    grandChildRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(grandChildRecordCmdListHandle, childJoinEventHandle, 1U, &childForkEventHandle);
    grandChildRecordCmdList.capture<CaptureApi::zeCommandListAppendSignalEvent>(grandChildRecordCmdListHandle, grandChildPostJoinSignalEventHandle);
    childRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(childRecordCmdListHandle, rootJoinEventHandle, 1U, &childJoinEventHandle);
    childRecordCmdList.capture<CaptureApi::zeCommandListAppendSignalEvent>(childRecordCmdListHandle, childPostJoinSignalEventHandle);
    rootRecordCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(rootRecordCmdListHandle, nullptr, 1U, &rootJoinEventHandle);
    srcGraph.stopCapturing();
    rootRecordCmdList.setGraphCaptureTarget(nullptr);

    ExposedExecutableGraph execGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph, settings));

    ASSERT_NE(nullptr, execGraph.trailingEventsPool);
    EXPECT_EQ(2u, childExecCmdList->appendSignalEventCalled);
    EXPECT_EQ(2u, grandChildExecCmdList->appendSignalEventCalled);
    ASSERT_EQ(2u, childExecCmdList->signaledEvents.size());
    ASSERT_EQ(2u, grandChildExecCmdList->signaledEvents.size());
    EXPECT_EQ(childPostJoinSignalEventHandle, childExecCmdList->signaledEvents[0]);
    EXPECT_EQ(grandChildPostJoinSignalEventHandle, grandChildExecCmdList->signaledEvents[0]);

    EXPECT_EQ(1u, rootExecCmdList->appendWaitOnEventsCalled);
    ASSERT_EQ(2u, rootExecCmdList->waitedEvents.size());

    std::unordered_set<ze_event_handle_t> expectedWaitEvents = {
        childExecCmdList->signaledEvents[1],
        grandChildExecCmdList->signaledEvents[1]};
    std::unordered_set<ze_event_handle_t> actualWaitEvents(rootExecCmdList->waitedEvents.begin(), rootExecCmdList->waitedEvents.end());
    EXPECT_EQ(expectedWaitEvents, actualWaitEvents);
}

TEST_F(GraphExecution, GivenGraphWithForkWhenInstantiatingToExecutableAndAppendReturnFailThenReturnCorrectErrorCode) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
    ctx.cmdListsToReturn[0]->appendWriteGlobalTimestampResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
    ctx.cmdListsToReturn[1]->appendWriteGlobalTimestampResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    mainRecordCmdlist.device = this->device;
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    mainRecordCmdlist.setGraphCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, signalEventParent);
    uint64_t *dstptr = nullptr;
    subCmdlist.capture<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(subCmdlistHandle, dstptr, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setGraphCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    static_cast<MockGraph *>(srcSubGraph)->captureTargetDesc.hDevice = device->toHandle();

    auto srcGraphHandle = srcGraph.toHandle();

    ze_executable_graph_handle_t physicalGraph = nullptr;
    auto err = L0::zeCommandListInstantiateGraphExp(srcGraphHandle, &physicalGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST_F(GraphExecution, GivenForceForkPolicyDebugKeyWhenCreateGraphSettingsThenFollowDebugKeyValue) {
    GraphsCleanupGuard graphCleanup;

    debugManager.flags.ForceGraphForkPolicy.set(GraphInstatiateSettings::ForkPolicy::ForkPolicyMonolythicLevels);
    bool multiEngine = false;

    GraphInstatiateSettings settings1(nullptr, multiEngine);
    EXPECT_EQ(GraphInstatiateSettings::ForkPolicy::ForkPolicyMonolythicLevels, settings1.forkPolicy);

    debugManager.flags.ForceGraphForkPolicy.set(GraphInstatiateSettings::ForkPolicy::ForkPolicySplitLevels);
    multiEngine = true;

    GraphInstatiateSettings settings2(nullptr, multiEngine);
    EXPECT_EQ(GraphInstatiateSettings::ForkPolicy::ForkPolicySplitLevels, settings2.forkPolicy);
}

TEST_F(GraphExecution, GivenExecutableGraphWithSubGraphsWhenSubmittingItToCommandListWithEventsThenUseEventsCorrectly) {
    struct MockGraphCommandListCapturingExecuteParams : MockGraphCmdListWithContext {
        using MockGraphCmdListWithContext::MockGraphCmdListWithContext;
        ze_result_t appendCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                                       ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                       CommandListExecutionInternalOptions &internalOptions) override {
            capturedParamsVec.push_back({numCommandLists, phCommandLists, hSignalEvent, numWaitEvents, phWaitEvents});
            return ZE_RESULT_SUCCESS;
        }
        struct CaptureParams {
            uint32_t numCommandLists;
            ze_command_list_handle_t *phCommandLists;
            ze_event_handle_t hSignalEvent;
            uint32_t numWaitEvents;
            ze_event_handle_t *phWaitEvents;
        };

        std::vector<CaptureParams> capturedParamsVec;
    };

    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCommandListCapturingExecuteParams mainExecCmdlist{&ctx};
    MockGraphCommandListCapturingExecuteParams subCmdlist{&ctx};
    Mock<CommandQueue> cmdQueue;
    mainRecordCmdlist.device = this->device;
    mainExecCmdlist.cmdQImmediate = &cmdQueue;
    mainExecCmdlist.device = this->device;
    subCmdlist.cmdQImmediate = &cmdQueue;
    subCmdlist.device = this->device;
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    mainRecordCmdlist.setGraphCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdlistHandle, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setGraphCaptureTarget(nullptr);

    GraphInstatiateSettings settings;

    Mock<Event> execWaitEvents[2];
    ze_event_handle_t execWaitEventHandles[2] = {&execWaitEvents[0], &execWaitEvents[1]};
    Mock<Event> signalEvent;
    {
        ExecutableGraph execMultiGraph;
        settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
        execMultiGraph.instantiateFrom(srcGraph, settings);

        auto res = execMultiGraph.execute(&mainExecCmdlist, nullptr, &signalEvent, 2, execWaitEventHandles);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        ASSERT_EQ(1U, mainExecCmdlist.capturedParamsVec.size());
        ASSERT_EQ(1U, subCmdlist.capturedParamsVec.size());

        EXPECT_EQ(2U, mainExecCmdlist.capturedParamsVec[0].numWaitEvents);
        EXPECT_EQ(execWaitEventHandles, mainExecCmdlist.capturedParamsVec[0].phWaitEvents);
        EXPECT_EQ(&signalEvent, mainExecCmdlist.capturedParamsVec[0].hSignalEvent);

        EXPECT_EQ(0U, subCmdlist.capturedParamsVec[0].numWaitEvents);
        EXPECT_EQ(nullptr, subCmdlist.capturedParamsVec[0].phWaitEvents);
        EXPECT_EQ(nullptr, subCmdlist.capturedParamsVec[0].hSignalEvent);
    }

    {
        ExecutableGraph execMultiGraph;
        mainExecCmdlist.capturedParamsVec.clear();
        subCmdlist.capturedParamsVec.clear();
        settings.forkPolicy = GraphInstatiateSettings::ForkPolicySplitLevels;
        execMultiGraph.instantiateFrom(srcGraph, settings);

        auto res = execMultiGraph.execute(&mainExecCmdlist, nullptr, &signalEvent, 2, execWaitEventHandles);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        ASSERT_EQ(2U, mainExecCmdlist.capturedParamsVec.size());
        ASSERT_EQ(1U, subCmdlist.capturedParamsVec.size());

        EXPECT_EQ(2U, mainExecCmdlist.capturedParamsVec[0].numWaitEvents);
        EXPECT_EQ(execWaitEventHandles, mainExecCmdlist.capturedParamsVec[0].phWaitEvents);
        EXPECT_EQ(nullptr, mainExecCmdlist.capturedParamsVec[0].hSignalEvent);

        EXPECT_EQ(0U, subCmdlist.capturedParamsVec[0].numWaitEvents);
        EXPECT_EQ(nullptr, subCmdlist.capturedParamsVec[0].phWaitEvents);
        EXPECT_EQ(nullptr, subCmdlist.capturedParamsVec[0].hSignalEvent);

        EXPECT_EQ(0U, mainExecCmdlist.capturedParamsVec[1].numWaitEvents);
        EXPECT_EQ(nullptr, mainExecCmdlist.capturedParamsVec[1].phWaitEvents);
        EXPECT_EQ(&signalEvent, mainExecCmdlist.capturedParamsVec[1].hSignalEvent);
    }

    mainExecCmdlist.cmdQImmediate = nullptr;
    subCmdlist.cmdQImmediate = nullptr;
}

TEST_F(GraphExecution, GivenExecutableGraphWithSingleSegmentWhenSegmentSubmissionFailsThenExecuteReturnsSameError) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandQueue> cmdQueue;
    Mock<CommandList> cmdlist;
    cmdlist.cmdQImmediate = &cmdQueue;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 0U, nullptr);
    srcGraph.stopCapturing();
    cmdlist.setGraphCaptureTarget(nullptr);

    srcGraph.captureTargetDesc.hDevice = device->toHandle();

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    cmdlist.appendCommandListsResult = ZE_RESULT_ERROR_DEVICE_LOST;

    auto res = execGraph.execute(&cmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);
    EXPECT_EQ(1U, cmdlist.appendCommandListsCalled);
    cmdlist.cmdQImmediate = nullptr;
}

TEST_F(GraphExecution, GivenExecutableGraphWithSubGraphsWhenFirstSegmentSubmissionFailsThenExecuteReturnsErrorWithoutSubmittingSubGraphSegment) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<CommandQueue> cmdQueue;
    mainRecordCmdlist.device = this->device;
    mainExecCmdlist.cmdQImmediate = &cmdQueue;
    mainExecCmdlist.device = this->device;
    subCmdlist.cmdQImmediate = &cmdQueue;
    subCmdlist.device = this->device;
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    mainRecordCmdlist.setGraphCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdlistHandle, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setGraphCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    execMultiGraph.instantiateFrom(srcGraph, settings);

    mainExecCmdlist.appendCommandListsResult = ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;

    auto res = execMultiGraph.execute(&mainExecCmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);
    EXPECT_EQ(1U, mainExecCmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, subCmdlist.appendCommandListsCalled);

    mainExecCmdlist.cmdQImmediate = nullptr;
    subCmdlist.cmdQImmediate = nullptr;
}

TEST_F(GraphExecution, GivenExecutableGraphWithSubGraphsWhenSubGraphSegmentSubmissionFailsThenExecuteReturnsSameError) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<CommandQueue> cmdQueue;
    mainRecordCmdlist.device = this->device;
    mainExecCmdlist.cmdQImmediate = &cmdQueue;
    mainExecCmdlist.device = this->device;
    subCmdlist.cmdQImmediate = &cmdQueue;
    subCmdlist.device = this->device;
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    mainRecordCmdlist.setGraphCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdlistHandle, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setGraphCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    execMultiGraph.instantiateFrom(srcGraph, settings);

    subCmdlist.appendCommandListsResult = ZE_RESULT_ERROR_DEVICE_LOST;

    auto res = execMultiGraph.execute(&mainExecCmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, res);
    EXPECT_EQ(1U, mainExecCmdlist.appendCommandListsCalled);
    EXPECT_EQ(1U, subCmdlist.appendCommandListsCalled);

    mainExecCmdlist.cmdQImmediate = nullptr;
    subCmdlist.cmdQImmediate = nullptr;
}

TEST_F(GraphExecution, GivenExecutableGraphSplitIntoThreeSegmentsWhenMiddleSegmentSubmissionFailsThenExecuteReturnsErrorWithoutSubmittingLastSegment) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<CommandQueue> cmdQueue;
    mainRecordCmdlist.device = this->device;
    mainExecCmdlist.cmdQImmediate = &cmdQueue;
    mainExecCmdlist.device = this->device;
    subCmdlist.cmdQImmediate = &cmdQueue;
    subCmdlist.device = this->device;
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    mainRecordCmdlist.setGraphCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdlistHandle, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setGraphCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicySplitLevels;
    execMultiGraph.instantiateFrom(srcGraph, settings);

    subCmdlist.appendCommandListsResult = ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;

    Mock<Event> signalEvent;
    auto res = execMultiGraph.execute(&mainExecCmdlist, nullptr, &signalEvent, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, res);
    EXPECT_EQ(1U, mainExecCmdlist.appendCommandListsCalled);
    EXPECT_EQ(1U, subCmdlist.appendCommandListsCalled);

    mainExecCmdlist.cmdQImmediate = nullptr;
    subCmdlist.cmdQImmediate = nullptr;
}

TEST_F(GraphExecution, GivenExecutableGraphSplitIntoThreeSegmentsWhenLastSegmentSubmissionFailsThenExecuteReturnsSameError) {
    struct MockGraphCmdListFailingAfterNthCall : MockGraphCmdListWithContext {
        using MockGraphCmdListWithContext::MockGraphCmdListWithContext;
        ze_result_t appendCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                                       ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                       CommandListExecutionInternalOptions &internalOptions) override {
            appendCommandListsCalled++;
            if (appendCommandListsCalled > callsToSucceed) {
                return failureResult;
            }
            return ZE_RESULT_SUCCESS;
        }
        uint32_t callsToSucceed = 0;
        ze_result_t failureResult = ZE_RESULT_SUCCESS;
    };

    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListFailingAfterNthCall mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<CommandQueue> cmdQueue;
    mainRecordCmdlist.device = this->device;
    mainExecCmdlist.cmdQImmediate = &cmdQueue;
    mainExecCmdlist.device = this->device;
    subCmdlist.cmdQImmediate = &cmdQueue;
    subCmdlist.device = this->device;
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    mainRecordCmdlist.setGraphCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdlistHandle, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setGraphCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicySplitLevels;
    execMultiGraph.instantiateFrom(srcGraph, settings);

    mainExecCmdlist.callsToSucceed = 1;
    mainExecCmdlist.failureResult = ZE_RESULT_ERROR_UNKNOWN;

    Mock<Event> signalEvent;
    auto res = execMultiGraph.execute(&mainExecCmdlist, nullptr, &signalEvent, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
    EXPECT_EQ(2U, mainExecCmdlist.appendCommandListsCalled);
    EXPECT_EQ(1U, subCmdlist.appendCommandListsCalled);

    mainExecCmdlist.cmdQImmediate = nullptr;
    subCmdlist.cmdQImmediate = nullptr;
}

TEST(ClosureExternalStorage, GivenEventWaitListThenRecordsItProperly) {
    MockEvent events[10];
    ze_event_handle_t eventHandles[10];
    std::transform(events, events + 10, eventHandles, [](auto &ev) { return ev.toHandle(); });

    L0::ClosureExternalStorage storage;
    EXPECT_EQ(L0::ClosureExternalStorage::invalidEventsListId, storage.registerEventsList(eventHandles, eventHandles));

    auto waitList0Id = storage.registerEventsList(eventHandles, eventHandles + 1);
    auto waitList1Id = storage.registerEventsList(eventHandles + 3, eventHandles + 5);
    auto waitList2Id = storage.registerEventsList(eventHandles + 8, eventHandles + 10);

    EXPECT_NE(L0::ClosureExternalStorage::invalidEventsListId, waitList0Id);
    EXPECT_NE(L0::ClosureExternalStorage::invalidEventsListId, waitList1Id);
    EXPECT_NE(L0::ClosureExternalStorage::invalidEventsListId, waitList2Id);

    EXPECT_EQ(nullptr, storage.getEventsList(L0::ClosureExternalStorage::invalidEventsListId));

    ASSERT_NE(nullptr, storage.getEventsList(waitList0Id));
    EXPECT_EQ(eventHandles[0], storage.getEventsList(waitList0Id)[0]);

    ASSERT_NE(nullptr, storage.getEventsList(waitList1Id));
    EXPECT_EQ(eventHandles[3], storage.getEventsList(waitList1Id)[0]);
    EXPECT_EQ(eventHandles[4], storage.getEventsList(waitList1Id)[1]);

    ASSERT_NE(nullptr, storage.getEventsList(waitList2Id));
    EXPECT_EQ(eventHandles[8], storage.getEventsList(waitList2Id)[0]);
    EXPECT_EQ(eventHandles[9], storage.getEventsList(waitList2Id)[1]);
}

TEST(ClosureExternalStorage, GivenImageRegionThenRecordsItProperly) {
    ze_image_region_t r1 = {};
    r1.width = 5;
    ze_image_region_t r2 = {};
    r2.width = 7;

    L0::ClosureExternalStorage storage;

    EXPECT_EQ(L0::ClosureExternalStorage::invalidImageRegionId, storage.registerImageRegion(nullptr));
    auto iamgeRegion1Id = storage.registerImageRegion(&r1);
    auto iamgeRegion2Id = storage.registerImageRegion(&r2);

    EXPECT_NE(L0::ClosureExternalStorage::invalidEventsListId, iamgeRegion1Id);
    EXPECT_NE(L0::ClosureExternalStorage::invalidEventsListId, iamgeRegion2Id);

    EXPECT_EQ(nullptr, storage.getImageRegion(L0::ClosureExternalStorage::invalidImageRegionId));

    ASSERT_NE(nullptr, storage.getImageRegion(iamgeRegion1Id));
    EXPECT_EQ(5U, storage.getImageRegion(iamgeRegion1Id)->width);

    ASSERT_NE(nullptr, storage.getImageRegion(iamgeRegion2Id));
    EXPECT_EQ(7U, storage.getImageRegion(iamgeRegion2Id)->width);
}

TEST(ClosureExternalStorage, GivenCopyRegionThenRecordsItProperly) {
    ze_copy_region_t r1 = {};
    r1.width = 5;
    ze_copy_region_t r2 = {};
    r2.width = 7;

    L0::ClosureExternalStorage storage;

    EXPECT_EQ(L0::ClosureExternalStorage::invalidCopyRegionId, storage.registerCopyRegion(nullptr));
    auto copyRegion1Id = storage.registerCopyRegion(&r1);
    auto copyRegion2Id = storage.registerCopyRegion(&r2);

    EXPECT_NE(L0::ClosureExternalStorage::invalidEventsListId, copyRegion1Id);
    EXPECT_NE(L0::ClosureExternalStorage::invalidEventsListId, copyRegion2Id);

    EXPECT_EQ(nullptr, storage.getCopyRegion(L0::ClosureExternalStorage::invalidCopyRegionId));

    ASSERT_NE(nullptr, storage.getCopyRegion(copyRegion1Id));
    EXPECT_EQ(5U, storage.getCopyRegion(copyRegion1Id)->width);

    ASSERT_NE(nullptr, storage.getCopyRegion(copyRegion2Id));
    EXPECT_EQ(7U, storage.getCopyRegion(copyRegion2Id)->width);
}

TEST_F(GraphTestCaptureRestrictions, GivenNonImmediateCommandListWhenBeginGraphCaptureCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    Mock<CommandList> regularCmdList;
    regularCmdList.cmdListType = L0::CommandList::CommandListType::typeRegular;
    auto cmdListHandle = regularCmdList.toHandle();

    auto err = L0::zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST_F(GraphTestCaptureRestrictions, GivenSynchronousImmediateCommandListWhenBeginGraphCaptureCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ze_command_list_handle_t syncCmdListHandle = nullptr;
    ze_command_queue_desc_t cmdQueueDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &syncCmdListHandle));

    auto err = L0::zeCommandListBeginGraphCaptureExp(syncCmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);

    zeCommandListDestroy(syncCmdListHandle);
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphWithUnjoinedForksWhenEndGraphCaptureCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext mainCmdlist{&ctx};
    mainCmdlist.device = this->device;
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;

    auto mainCmdlistHandle = mainCmdlist.toHandle();
    auto forkEventHandle = forkEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    mainCmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(mainCmdlist, false);

    mainCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainCmdlistHandle, forkEventHandle, 0U, nullptr);
    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, forkEvent);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&subCmdlist, nullptr, 1U, &forkEventHandle);

    ze_graph_handle_t retGraph = nullptr;
    auto err = L0::zeCommandListEndGraphCaptureExp(mainCmdlistHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_UNJOINED_FORKS, err);
    EXPECT_EQ(srcGraph.toHandle(), retGraph); // graph is still returned, so that the caller can destroy it
    EXPECT_FALSE(srcGraph.valid());
    EXPECT_FALSE(mainCmdlist.isCapturingGraph());
    EXPECT_FALSE(subCmdlist.isCapturingGraph());

    // try to use graph with unjoined fork
    ze_executable_graph_handle_t execGraph = nullptr;
    err = L0::zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GRAPH, err);
    EXPECT_EQ(nullptr, execGraph);

    if (nullptr != execGraph) {
        L0::zeExecutableGraphDestroyExp(execGraph);
    }
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphWithUnjoinedForksWhenEndGraphCaptureCalledWithoutOutputHandleThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext mainCmdlist{&ctx};
    mainCmdlist.device = this->device;
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;

    auto mainCmdlistHandle = mainCmdlist.toHandle();
    auto forkEventHandle = forkEvent.toHandle();

    Graph srcGraph(&ctx, true, false); // preallocated, so that no output handle is needed when ending capture
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExt(mainCmdlistHandle, srcGraph.toHandle(), nullptr));

    Graph *mainCaptureTarget = &srcGraph;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdlist, mainCaptureTarget, nullptr, mainCmdlistHandle, forkEventHandle, 0U, nullptr);

    Graph *subCaptureTarget = nullptr;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(subCmdlist, subCaptureTarget, nullptr, &subCmdlist, nullptr, 1U, &forkEventHandle);
    ASSERT_NE(nullptr, subCaptureTarget); // forked, but never joined back

    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_UNJOINED_FORKS, L0::zeCommandListEndGraphCaptureExt(mainCmdlistHandle, nullptr, nullptr));
    EXPECT_FALSE(srcGraph.valid());
    EXPECT_FALSE(mainCmdlist.isCapturingGraph());
    EXPECT_FALSE(subCmdlist.isCapturingGraph());
}

TEST_F(GraphTestCaptureRestrictions, GivenSubgraphWithUnjoinedForksWhenEndGraphCaptureCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext mainCmdlist{&ctx};
    mainCmdlist.device = this->device;
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    MockGraphCmdListWithContext grandChildCmdlist{&ctx};
    grandChildCmdlist.device = this->device;
    Mock<Event> forkToChildEvent;
    Mock<Event> forkToGrandChildEvent;
    Mock<Event> joinChildEvent;

    auto mainCmdlistHandle = mainCmdlist.toHandle();
    auto forkToChildEventHandle = forkToChildEvent.toHandle();
    auto forkToGrandChildEventHandle = forkToGrandChildEvent.toHandle();
    auto joinChildEventHandle = joinChildEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *mainCaptureTarget = &srcGraph;
    mainCmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(mainCmdlist, false);

    // lvl 0 : fork to child
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdlist, mainCaptureTarget, nullptr, mainCmdlistHandle, forkToChildEventHandle, 0U, nullptr);

    // lvl 1 : child forks to grandchild
    Graph *childCaptureTarget = nullptr;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childCaptureTarget, nullptr, &childCmdlist, forkToGrandChildEventHandle, 1U, &forkToChildEventHandle);
    ASSERT_NE(nullptr, childCaptureTarget);

    // lvl 2 : grandchild is never joined back to the child
    Graph *grandChildCaptureTarget = nullptr;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildCaptureTarget, nullptr, &grandChildCmdlist, nullptr, 1U, &forkToGrandChildEventHandle);
    ASSERT_NE(nullptr, grandChildCaptureTarget);

    // lvl 1 -> lvl 0 : child is properly joined back
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childCaptureTarget, nullptr, &childCmdlist, joinChildEventHandle, 0U, nullptr);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdlist, mainCaptureTarget, nullptr, mainCmdlistHandle, nullptr, 1U, &joinChildEventHandle);

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_UNJOINED_FORKS, L0::zeCommandListEndGraphCaptureExt(mainCmdlistHandle, nullptr, &retGraph));
    EXPECT_EQ(srcGraph.toHandle(), retGraph);
    EXPECT_TRUE(srcGraph.getUnjoinedForks().empty()); // the unjoined fork is in the subgraph
    EXPECT_FALSE(srcGraph.valid());
}

TEST_F(GraphTestCaptureRestrictions, GivenGrandchildForkJoinedTransitivelyToGrandparentWhenEndGraphCaptureCalledThenGraphIsValid) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext mainCmdlist{&ctx};
    mainCmdlist.device = this->device;
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    MockGraphCmdListWithContext grandChildCmdlist{&ctx};
    grandChildCmdlist.device = this->device;
    Mock<Event> forkToChildEvent;
    Mock<Event> forkToGrandChildEvent;
    Mock<Event> joinGrandChildToMainEvent;
    Mock<Event> joinChildToMainEvent;

    auto mainCmdlistHandle = mainCmdlist.toHandle();
    auto forkToChildEventHandle = forkToChildEvent.toHandle();
    auto forkToGrandChildEventHandle = forkToGrandChildEvent.toHandle();
    auto joinGrandChildToMainEventHandle = joinGrandChildToMainEvent.toHandle();
    auto joinChildToMainEventHandle = joinChildToMainEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *mainCaptureTarget = &srcGraph;
    mainCmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(mainCmdlist, false);

    // lvl 0 : fork to child
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdlist, mainCaptureTarget, nullptr, mainCmdlistHandle, forkToChildEventHandle, 0U, nullptr);

    // lvl 1 : child forks to grandchild
    Graph *childCaptureTarget = nullptr;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childCaptureTarget, nullptr, &childCmdlist, forkToGrandChildEventHandle, 1U, &forkToChildEventHandle);
    ASSERT_NE(nullptr, childCaptureTarget);

    // lvl 2 : grandchild signals its join event as its last work command
    Graph *grandChildCaptureTarget = nullptr;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildCaptureTarget, nullptr, &grandChildCmdlist, joinGrandChildToMainEventHandle, 1U, &forkToGrandChildEventHandle);
    ASSERT_NE(nullptr, grandChildCaptureTarget);

    // lvl 2 -> lvl 0 : grandchild is joined transitively, directly to the grandparent (main), skipping the child
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdlist, mainCaptureTarget, nullptr, mainCmdlistHandle, nullptr, 1U, &joinGrandChildToMainEventHandle);

    // lvl 1 -> lvl 0 : child is joined back to its direct parent
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childCaptureTarget, nullptr, &childCmdlist, joinChildToMainEventHandle, 0U, nullptr);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(mainCmdlist, mainCaptureTarget, nullptr, mainCmdlistHandle, nullptr, 1U, &joinChildToMainEventHandle);

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(mainCmdlistHandle, nullptr, &retGraph));
    EXPECT_EQ(srcGraph.toHandle(), retGraph);
    EXPECT_TRUE(srcGraph.valid());
    EXPECT_TRUE(srcGraph.getUnjoinedForks().empty());
    ASSERT_EQ(1u, srcGraph.getSubgraphs().size());
    const auto *childGraph = srcGraph.getSubgraphs()[0];
    EXPECT_TRUE(childGraph->getUnjoinedForks().empty()); // grandchild's fork was resolved on the child even though main performed the join
    EXPECT_EQ(1u, childGraph->getResolvedJoins().size());
    ASSERT_EQ(1u, childGraph->getSubgraphs().size());
    EXPECT_TRUE(childGraph->getSubgraphs()[0]->getUnjoinedForks().empty());
    EXPECT_FALSE(mainCmdlist.isCapturingGraph());
    EXPECT_FALSE(childCmdlist.isCapturingGraph());
    EXPECT_FALSE(grandChildCmdlist.isCapturingGraph());
}

TEST_F(GraphTestCaptureRestrictions, GivenForkJoinedTransitivelyThroughSiblingWhenEndGraphCaptureCalledThenGraphIsValid) {
    GraphsCleanupGuard graphCleanup;

    // Reproduces the transitive-join topology where a forked queue is joined through a sibling:
    //   qcap forks q1 and q2; q1 signals to q2 (neither end is qcap); q2 is joined back to qcap.
    // q2 is joined directly to qcap and q1 is joined transitively through q2.
    ContextStubMock ctx;
    MockGraphCmdListWithContext qcap{&ctx};
    qcap.device = this->device;
    MockGraphCmdListWithContext q1{&ctx};
    q1.device = this->device;
    MockGraphCmdListWithContext q2{&ctx};
    q2.device = this->device;
    Mock<Event> e1; // fork qcap -> q1
    Mock<Event> e2; // fork qcap -> q2
    Mock<Event> e3; // join q1 -> q2 (offending edge: neither end is qcap)
    Mock<Event> e4; // join q2 -> qcap

    auto qcapHandle = qcap.toHandle();
    auto e1Handle = e1.toHandle();
    auto e2Handle = e2.toHandle();
    auto e3Handle = e3.toHandle();
    auto e4Handle = e4.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *qcapCaptureTarget = &srcGraph;
    qcap.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(qcap, false);

    // qcap forks q1
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(qcap, qcapCaptureTarget, nullptr, qcapHandle, e1Handle, 0U, nullptr);
    Graph *q1CaptureTarget = nullptr;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(q1, q1CaptureTarget, nullptr, &q1, nullptr, 1U, &e1Handle);
    ASSERT_NE(nullptr, q1CaptureTarget);

    // qcap forks q2
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(qcap, qcapCaptureTarget, nullptr, qcapHandle, e2Handle, 0U, nullptr);
    Graph *q2CaptureTarget = nullptr;
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(q2, q2CaptureTarget, nullptr, &q2, nullptr, 1U, &e2Handle);
    ASSERT_NE(nullptr, q2CaptureTarget);

    // q1 signals its join event as its last work command
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(q1, q1CaptureTarget, nullptr, &q1, e3Handle, 0U, nullptr);

    // q1 -> q2 : q1 is joined onto its sibling q2 (neither end is the primary command list)
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(q2, q2CaptureTarget, nullptr, &q2, nullptr, 1U, &e3Handle);

    // q2 signals its join event as its last work command
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(q2, q2CaptureTarget, nullptr, &q2, e4Handle, 0U, nullptr);

    // q2 -> qcap : q2 is joined directly back to the primary command list
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(qcap, qcapCaptureTarget, nullptr, qcapHandle, nullptr, 1U, &e4Handle);

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(qcapHandle, nullptr, &retGraph));
    EXPECT_EQ(srcGraph.toHandle(), retGraph);
    EXPECT_TRUE(srcGraph.valid());
    EXPECT_TRUE(srcGraph.getUnjoinedForks().empty()); // both sibling forks were resolved on their owner (qcap)
    EXPECT_EQ(2u, srcGraph.getResolvedJoins().size());
    ASSERT_EQ(2u, srcGraph.getSubgraphs().size());
    EXPECT_TRUE(srcGraph.getSubgraphs()[0]->getUnjoinedForks().empty());
    EXPECT_TRUE(srcGraph.getSubgraphs()[1]->getUnjoinedForks().empty());
    EXPECT_FALSE(qcap.isCapturingGraph());
    EXPECT_FALSE(q1.isCapturingGraph());
    EXPECT_FALSE(q2.isCapturingGraph());
}

TEST_F(GraphTestCaptureRestrictions, GivenEventFromOtherCaptureSessionWhenCapturingCommandListWaitsOnItThenMergeAttemptErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlistA{&ctx};
    cmdlistA.device = this->device;
    MockGraphCmdListWithContext cmdlistB{&ctx};
    cmdlistB.device = this->device;
    Mock<Event> eventFromB;

    auto cmdlistAHandle = cmdlistA.toHandle();
    auto cmdlistBHandle = cmdlistB.toHandle();
    auto eventFromBHandle = eventFromB.toHandle();

    // two independent capture sessions, each with its own primary command list
    Graph graphA(&ctx, true, false);
    Graph *captureTargetA = &graphA;
    cmdlistA.setGraphCaptureTarget(&graphA);
    graphA.startCapturingFrom(cmdlistA, false);

    Graph graphB(&ctx, true, false);
    Graph *captureTargetB = &graphB;
    cmdlistB.setGraphCaptureTarget(&graphB);
    graphB.startCapturingFrom(cmdlistB, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlistB, captureTargetB, nullptr, cmdlistBHandle, eventFromBHandle, 0U, nullptr));

    // waiting in session A on an event recorded in session B would merge both graphs
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_MERGE_ATTEMPT, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlistA, captureTargetA, nullptr, cmdlistAHandle, nullptr, 1U, &eventFromBHandle));
    EXPECT_TRUE(graphA.getCapturedCommands().empty());  // nothing was recorded into the waiting session
    EXPECT_EQ(1U, graphB.getCapturedCommands().size()); // nor into the signalling one
    EXPECT_TRUE(graphA.getSubgraphs().empty());         // and neither session was forked into the other
    EXPECT_TRUE(graphB.getSubgraphs().empty());

    cmdlistA.setGraphCaptureTarget(nullptr);
    cmdlistB.setGraphCaptureTarget(nullptr);
}

TEST_F(GraphTestCaptureRestrictions, GivenEventsFromOwnAndOtherCaptureSessionWhenCapturingCommandListWaitsOnThemThenMergeAttemptErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlistA{&ctx};
    cmdlistA.device = this->device;
    MockGraphCmdListWithContext cmdlistB{&ctx};
    cmdlistB.device = this->device;
    Mock<Event> eventFromA;
    Mock<Event> eventFromB;

    auto cmdlistAHandle = cmdlistA.toHandle();
    auto cmdlistBHandle = cmdlistB.toHandle();
    auto eventFromAHandle = eventFromA.toHandle();
    auto eventFromBHandle = eventFromB.toHandle();

    Graph graphA(&ctx, true, false);
    Graph *captureTargetA = &graphA;
    cmdlistA.setGraphCaptureTarget(&graphA);
    graphA.startCapturingFrom(cmdlistA, false);

    Graph graphB(&ctx, true, false);
    Graph *captureTargetB = &graphB;
    cmdlistB.setGraphCaptureTarget(&graphB);
    graphB.startCapturingFrom(cmdlistB, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlistA, captureTargetA, nullptr, cmdlistAHandle, eventFromAHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlistB, captureTargetB, nullptr, cmdlistBHandle, eventFromBHandle, 0U, nullptr));

    // the foreign event must be detected regardless of its position in the wait list
    ze_event_handle_t ownEventFirst[] = {eventFromAHandle, eventFromBHandle};
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_MERGE_ATTEMPT, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlistA, captureTargetA, nullptr, cmdlistAHandle, nullptr, 2U, ownEventFirst));

    ze_event_handle_t foreignEventFirst[] = {eventFromBHandle, eventFromAHandle};
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_MERGE_ATTEMPT, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlistA, captureTargetA, nullptr, cmdlistAHandle, nullptr, 2U, foreignEventFirst));

    EXPECT_EQ(1U, graphA.getCapturedCommands().size()); // only the initial signalling barrier was recorded

    cmdlistA.setGraphCaptureTarget(nullptr);
    cmdlistB.setGraphCaptureTarget(nullptr);
}

TEST_F(GraphTestCaptureRestrictions, GivenEventFromSameCaptureSessionWhenChildCommandListWaitsOnItThenCaptureIsContinued) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext parentCmdlist{&ctx};
    parentCmdlist.device = this->device;
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    Mock<Event> nextParentEvent;
    Mock<Event> joinEvent;

    auto parentCmdlistHandle = parentCmdlist.toHandle();
    auto forkEventHandle = forkEvent.toHandle();
    auto nextParentEventHandle = nextParentEvent.toHandle();
    auto joinEventHandle = joinEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *parentCaptureTarget = &srcGraph;
    parentCmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(parentCmdlist, false);

    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(parentCmdlist, parentCaptureTarget, nullptr, parentCmdlistHandle, forkEventHandle, 0U, nullptr);

    Graph *childCaptureTarget = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childCaptureTarget, nullptr, &childCmdlist, nullptr, 1U, &forkEventHandle));
    ASSERT_NE(nullptr, childCaptureTarget);
    ASSERT_NE(&srcGraph, childCaptureTarget);                 // child records into a subgraph ...
    EXPECT_EQ(&srcGraph, childCaptureTarget->getRootGraph()); // ... of the same capture session

    // subsequent signals from the parent to the child stay within the same session and are not a merge attempt
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(parentCmdlist, parentCaptureTarget, nullptr, parentCmdlistHandle, nextParentEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childCaptureTarget, nullptr, &childCmdlist, nullptr, 1U, &nextParentEventHandle));
    EXPECT_EQ(2U, childCaptureTarget->getCapturedCommands().size());

    // join the fork back, so that the whole session is still a valid graph
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childCaptureTarget, nullptr, &childCmdlist, joinEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(parentCmdlist, parentCaptureTarget, nullptr, parentCmdlistHandle, nullptr, 1U, &joinEventHandle));

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(parentCmdlistHandle, nullptr, &retGraph));
    EXPECT_EQ(srcGraph.toHandle(), retGraph);
    EXPECT_TRUE(srcGraph.valid());
}

TEST_F(GraphTestCaptureRestrictions, GivenCommandListAlreadyCapturingWhenBeginGraphCaptureCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    Mock<CommandList> cmdList;
    cmdList.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdList.device = this->device;
    auto cmdListHandle = cmdList.toHandle();

    auto err = L0::zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);

    // check also zeCommandListBeginCaptureIntoGraphExp
    Graph graph(&ctx, true, false);
    auto graphHandle = graph.toHandle();
    err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);

    ze_graph_handle_t retGraph = nullptr;
    err = L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    L0::zeGraphDestroyExp(retGraph);
}

TEST_F(GraphTestCaptureRestrictions, GivenCommandListNotCapturingWhenEndGraphCaptureCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ze_graph_handle_t retGraph = nullptr;
    auto err = L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST_F(GraphTestCaptureRestrictions, GivenNonEmptyGraphWhenBeginCaptureIntoGraphCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdList{&ctx};
    cmdList.device = this->device;
    Mock<Event> event;

    auto cmdListHandle = cmdList.toHandle();
    auto eventHandle = event.toHandle();
    auto ctxHandle = ctx.toHandle();

    ze_graph_handle_t graphHandle = nullptr;
    auto err = L0::zeGraphCreateExp(ctxHandle, &graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    ASSERT_NE(nullptr, graphHandle);

    auto *graph = L0::Graph::fromHandle(graphHandle);
    cmdList.setGraphCaptureTarget(graph);
    graph->startCapturingFrom(cmdList, false);

    cmdList.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, eventHandle, 0U, nullptr);

    graph->stopCapturing();
    cmdList.setGraphCaptureTarget(nullptr);

    MockGraphCmdListWithContext cmdList2{&ctx};
    auto cmdList2Handle = cmdList2.toHandle();

    err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdList2Handle, graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, cmdList2.getGraphCaptureTarget());

    L0::zeGraphDestroyExp(graphHandle);
}

TEST_F(GraphTestCaptureRestrictions, GivenRecordedEventWhenWaitedOnByNotCapturableCommandListThenCommandListTypeErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    auto internalEventHandle = createCounterBasedEvent(context, device, false);
    auto externalEventHandle = createCounterBasedEvent(context, device, true);

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, internalEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, externalEventHandle, 0U, nullptr));
    ASSERT_TRUE(L0::Event::fromHandle(internalEventHandle)->isCapturedGraphInternalEvent());
    ASSERT_FALSE(L0::Event::fromHandle(externalEventHandle)->isCapturedGraphInternalEvent());

    Mock<CommandList> regularCmdList;
    regularCmdList.cmdListType = L0::CommandList::CommandListType::typeRegular;
    regularCmdList.device = this->device;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE,
              regularCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(regularCmdList.toHandle(), nullptr, 1U, &internalEventHandle));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE,
              regularCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(regularCmdList.toHandle(), nullptr, 1U, &externalEventHandle));

    ze_event_handle_t internalFirst[] = {internalEventHandle, externalEventHandle};
    ze_event_handle_t externalFirst[] = {externalEventHandle, internalEventHandle};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE,
              regularCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(regularCmdList.toHandle(), nullptr, 2U, internalFirst));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE,
              regularCmdList.capture<CaptureApi::zeCommandListAppendBarrier>(regularCmdList.toHandle(), nullptr, 2U, externalFirst));

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(internalEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(externalEventHandle));
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphInternalEventWhenSignalledByNotCapturableCommandListThenEventTransitionsToEagerState) {
    GraphsCleanupGuard graphCleanup;

    auto internalEventHandle = createCounterBasedEvent(context, device, false);
    auto externalEventHandle = createCounterBasedEvent(context, device, true);

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, internalEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, externalEventHandle, 0U, nullptr));

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> regularCmdList(CommandList::create(device, NEO::EngineGroupType::compute, ZE_COMMAND_LIST_FLAG_IN_ORDER, returnValue, false));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto regularCmdListHandle = regularCmdList->toHandle();

    EXPECT_TRUE(L0::Event::fromHandle(internalEventHandle)->isGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(regularCmdListHandle, internalEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(regularCmdListHandle, externalEventHandle));
    EXPECT_FALSE(L0::Event::fromHandle(internalEventHandle)->isGraphInternalEvent());
    EXPECT_EQ(nullptr, L0::Event::fromHandle(internalEventHandle)->getRecordedSignalFrom());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(regularCmdListHandle, 1U, &internalEventHandle));

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(internalEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(externalEventHandle));
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphInternalEventWhenSignalledByNotCapturingCommandListThenEventTransitionsToEagerState) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, false);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t cmdQueueDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    std::unique_ptr<L0::CommandList> otherCmdList(CommandList::createImmediate(device, &cmdQueueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto otherCmdListHandle = otherCmdList->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, eventHandle, 0U, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(otherCmdListHandle, eventHandle));
    EXPECT_FALSE(L0::Event::fromHandle(eventHandle)->isGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(otherCmdListHandle, 1U, &eventHandle));
    EXPECT_FALSE(otherCmdList->isCapturingGraph());

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphInternalEventWhenEagerAppendSignallingItFailsValidationThenEventStaysGraphInternal) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(eventHandle);

    ze_command_list_handle_t otherCmdListHandle = nullptr;
    ze_command_queue_desc_t cmdQueueDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &otherCmdListHandle));

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, eventHandle, 0U, nullptr));

    constexpr size_t nonPow2PatternSize = 3U;
    uint8_t pattern[nonPow2PatternSize] = {};
    uint8_t buffer[nonPow2PatternSize * 4] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zeCommandListAppendMemoryFill(otherCmdListHandle, buffer, pattern, nonPow2PatternSize, sizeof(buffer), eventHandle, 0U, nullptr));
    EXPECT_TRUE(event->isGraphInternalEvent());
    EXPECT_EQ(immCmdList, event->getRecordedSignalFrom());
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(otherCmdListHandle, nullptr, 1U, &eventHandle));
    EXPECT_TRUE(L0::CommandList::fromHandle(otherCmdListHandle)->isCapturingGraph());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(otherCmdListHandle, eventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 1U, &eventHandle));

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &retGraph));
    EXPECT_TRUE(L0::Graph::fromHandle(retGraph)->validForInstantiation());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(otherCmdListHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphInternalEventWhenSignalledEagerlyBetweenForkAndJoinThenGraphKeepsForkAndJoinAndEagerWaitSucceeds) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(eventHandle);

    ze_command_list_handle_t childCmdListHandle = nullptr;
    ze_command_queue_desc_t cmdQueueDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &childCmdListHandle));

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> eagerSignalCmdList(CommandList::createImmediate(device, &cmdQueueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    std::unique_ptr<L0::CommandList> eagerWaitCmdList(CommandList::createImmediate(device, &cmdQueueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto eagerSignalCmdListHandle = eagerSignalCmdList->toHandle();
    auto eagerWaitCmdListHandle = eagerWaitCmdList->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, eventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(childCmdListHandle, nullptr, 1U, &eventHandle));
    EXPECT_TRUE(L0::CommandList::fromHandle(childCmdListHandle)->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(eagerSignalCmdListHandle, eventHandle));
    EXPECT_FALSE(event->isGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(eagerWaitCmdListHandle, 1U, &eventHandle));
    EXPECT_FALSE(eagerSignalCmdList->isCapturingGraph());
    EXPECT_FALSE(eagerWaitCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(childCmdListHandle, eventHandle, 0U, nullptr));
    EXPECT_TRUE(event->isGraphInternalEvent());
    EXPECT_EQ(L0::CommandList::fromHandle(childCmdListHandle), event->getRecordedSignalFrom());
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 1U, &eventHandle));

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &retGraph));
    auto *graph = L0::Graph::fromHandle(retGraph);
    EXPECT_TRUE(graph->validForInstantiation());
    EXPECT_EQ(1U, graph->getSubgraphs().size());
    EXPECT_EQ(1U, graph->getResolvedJoins().size());
    EXPECT_FALSE(L0::CommandList::fromHandle(childCmdListHandle)->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(childCmdListHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphInternalEventWhenSignalledInOtherActiveCaptureThenEventBecomesInternalToOtherCapture) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(eventHandle);

    ze_command_list_handle_t otherCaptureCmdListHandle = nullptr;
    ze_command_list_handle_t forkedCmdListHandle = nullptr;
    ze_command_queue_desc_t cmdQueueDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &otherCaptureCmdListHandle));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &forkedCmdListHandle));
    auto *otherCaptureCmdList = L0::CommandList::fromHandle(otherCaptureCmdListHandle);
    auto *forkedCmdList = L0::CommandList::fromHandle(forkedCmdListHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(otherCaptureCmdListHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, eventHandle, 0U, nullptr));
    EXPECT_EQ(immCmdList, event->getRecordedSignalFrom());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(otherCaptureCmdListHandle, eventHandle, 0U, nullptr));
    EXPECT_EQ(otherCaptureCmdList, event->getRecordedSignalFrom());
    EXPECT_TRUE(event->isGraphInternalEvent());

    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_MERGE_ATTEMPT, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 1U, &eventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(forkedCmdListHandle, nullptr, 1U, &eventHandle));
    ASSERT_TRUE(forkedCmdList->isCapturingGraph());
    EXPECT_EQ(otherCaptureCmdList->getGraphCaptureTarget(), forkedCmdList->getGraphCaptureTarget()->getRootGraph());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(forkedCmdListHandle, eventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(otherCaptureCmdListHandle, nullptr, 1U, &eventHandle));

    ze_graph_handle_t firstGraph = nullptr;
    ze_graph_handle_t otherGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &firstGraph));
    EXPECT_TRUE(event->isGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(otherCaptureCmdListHandle, nullptr, &otherGraph));
    EXPECT_FALSE(event->isCapturedGraphInternalEvent());
    EXPECT_TRUE(L0::Graph::fromHandle(firstGraph)->validForInstantiation());
    EXPECT_TRUE(L0::Graph::fromHandle(otherGraph)->validForInstantiation());
    EXPECT_EQ(1U, L0::Graph::fromHandle(otherGraph)->getResolvedJoins().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(firstGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(otherGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(otherCaptureCmdListHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(forkedCmdListHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
}

TEST_F(GraphTestCaptureRestrictions, GivenGraphInternalEventWhenUsedForForkAndJoinWithinOneCaptureThenCaptureSucceeds) {
    GraphsCleanupGuard graphCleanup;

    auto forkEventHandle = createCounterBasedEvent(context, device, false);
    auto joinEventHandle = createCounterBasedEvent(context, device, false);

    ze_command_list_handle_t childCmdListHandle = nullptr;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &childCmdListHandle));

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, forkEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(childCmdListHandle, joinEventHandle, 1U, &forkEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 1U, &joinEventHandle));
    EXPECT_TRUE(L0::CommandList::fromHandle(childCmdListHandle)->isCapturingGraph());

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &retGraph));

    EXPECT_FALSE(L0::Event::fromHandle(forkEventHandle)->isCapturedGraphInternalEvent());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(childCmdListHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(forkEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(joinEventHandle));
}

TEST_F(GraphTestCaptureRestrictions, GivenHostSynchronizeWhenCapturingCmdlistThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    auto err = L0::zeCommandListBeginGraphCaptureExp(immCmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = immCmdList->hostSynchronize(0);
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED, err);
}

TEST_F(GraphTestCaptureRestrictions, GivenImplicitlyCounterBasedEventSignalledByCaptureWhenSignalledByNotInOrderCommandListThenRecordedSignalIsReleased) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.set(1);

    ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
    ze_event_pool_handle_t hPool = nullptr;
    auto hDevice = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &poolDesc, 1, &hDevice, &hPool));

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
    ze_event_handle_t hEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventDesc, &hEvent));
    auto *event = L0::Event::fromHandle(hEvent);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t inOrderQueueDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    std::unique_ptr<L0::CommandList> inOrderCmdList(CommandList::createImmediate(device, &inOrderQueueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ze_command_queue_desc_t notInOrderQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    std::unique_ptr<L0::CommandList> notInOrderCmdList(CommandList::createImmediate(device, &notInOrderQueueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(inOrderCmdList->toHandle(), hEvent));
    ASSERT_TRUE(event->isCounterBased());
    ASSERT_FALSE(event->isCounterBasedExplicitlyEnabled());

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExt(immCmdListHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, hEvent, 0U, nullptr));
    EXPECT_EQ(immCmdList, event->getRecordedSignalFrom());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(notInOrderCmdList->toHandle(), hEvent));
    EXPECT_FALSE(event->isCounterBased());
    EXPECT_EQ(nullptr, event->getRecordedSignalFrom());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED, event->hostSynchronize(0));

    ze_graph_handle_t retGraph = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExt(immCmdListHandle, nullptr, &retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExt(retGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(hPool));
}

TEST_F(GraphTestCaptureRestrictions, GivenEventSignalledByCapturingCmdlistWhenEventHostSynchronizeCalledThenGraphCaptureUnsupportedReturned) {
    GraphsCleanupGuard graphCleanup;

    ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 1};
    ze_event_pool_handle_t hPool = nullptr;
    auto devHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &poolDesc, 1, &devHandle, &hPool));

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
    ze_event_handle_t hEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventDesc, &hEvent));
    auto *event = L0::Event::fromHandle(hEvent);

    auto signalEntry = std::make_shared<GraphInternalEventEntry>(GraphInternalEventEntry{.signalFrom = immCmdList});
    event->setRecordedSignalEntry(signalEntry);

    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED, event->hostSynchronize(0));

    event->setRecordedSignalEntry({});
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED, event->hostSynchronize(0));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(hPool));
}

TEST_F(GraphTestCaptureRestrictions, GivenInternalCbEventSignalledByCapturingCmdlistWhenEventQueryStatusCalledThenGraphInternalEventReturned) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(eventHandle);

    auto signalEntry = std::make_shared<GraphInternalEventEntry>(GraphInternalEventEntry{.signalFrom = immCmdList});
    event->setRecordedSignalEntry(signalEntry);
    EXPECT_TRUE(event->isCapturedGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    event->setRecordedSignalEntry({});
    EXPECT_FALSE(event->isCapturedGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    event->destroy();
}

TEST_F(GraphTestCaptureRestrictions, GivenRegularEventSignalledByCapturingCmdlistWhenEventQueryStatusCalledThenStatusIsReported) {
    GraphsCleanupGuard graphCleanup;

    ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 1};
    ze_event_pool_handle_t hPool = nullptr;
    auto hDevice = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &poolDesc, 1, &hDevice, &hPool));

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
    ze_event_handle_t hEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventDesc, &hEvent));
    auto *event = L0::Event::fromHandle(hEvent);

    auto signalEntry = std::make_shared<GraphInternalEventEntry>(GraphInternalEventEntry{.signalFrom = immCmdList});
    event->setRecordedSignalEntry(signalEntry);
    EXPECT_FALSE(event->isCapturedGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(hEvent));

    event->setRecordedSignalEntry({});

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(hPool));
}

TEST(GraphTestZeCommandListGetGraphExp, GivenInvalidParametersThenReturnsAppropriateErrorCode) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockCommandList cmdlist;
    MockGraph graph{&ctx, true, false};
    ze_graph_handle_t queryResult = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeCommandListGetGraphExp(nullptr, &queryResult)) << "Empty cmdlist is invalid";
    EXPECT_EQ(ZE_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING, L0::zeCommandListGetGraphExp(&cmdlist, &queryResult)) << "Non recording cmdlist is invalid";
    cmdlist.setGraphCaptureTarget(&graph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetGraphExp(&cmdlist, &queryResult));
    EXPECT_EQ(&graph, queryResult) << "Recording cmdlist should return target graph";
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeCommandListGetGraphExp(&cmdlist, nullptr)) << "Return parameter cannot be null";
    cmdlist.setGraphCaptureTarget(nullptr);
}

TEST(GraphTestZeCommandListGetGraphExp, GivenValidParametersThenReturnsUnderlyingGraph) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockCommandList cmdlist;
    MockGraph graph{&ctx, true, false};
    ze_graph_handle_t queryResult = {};

    cmdlist.setGraphCaptureTarget(&graph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetGraphExp(&cmdlist, &queryResult));
    EXPECT_EQ(&graph, queryResult) << "Recording cmdlist should return target graph";
    cmdlist.setGraphCaptureTarget(nullptr);
}

struct TestableGraphForCmdListQuery : public L0::Graph {
    using L0::Graph::Graph;

    void setParentGraph(Graph *graph) {
        parentGraph = graph;
    }

    void setExecutionTarget(L0::CommandList *cmdList) {
        executionTarget = cmdList;
    }
};

TEST(GraphTestZeCommandListGetGraphExp, GivenForkedCommandListThenReturnsParentGraph) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph parentGraph{&ctx, true, false};
    TestableGraphForCmdListQuery subgraph{&ctx, true, false};
    MockCommandList parentCmdlist;
    MockCommandList childCmdlist;
    ze_graph_handle_t queryResult = {};

    // Simulate a parent command list capturing to parent graph
    parentCmdlist.setGraphCaptureTarget(&parentGraph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetGraphExp(&parentCmdlist, &queryResult));
    EXPECT_EQ(&parentGraph, queryResult) << "Parent cmdlist should return parent graph";

    // Simulate forking: create a subgraph and set it as capture target for child
    subgraph.setParentGraph(&parentGraph);
    subgraph.setExecutionTarget(&childCmdlist);
    childCmdlist.setGraphCaptureTarget(&subgraph);

    // When querying the child command list, should return parent graph, not subgraph
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetGraphExp(&childCmdlist, &queryResult));
    EXPECT_EQ(&parentGraph, queryResult) << "Forked cmdlist should return parent graph, not subgraph";
    EXPECT_NE(&subgraph, queryResult) << "Should not return subgraph handle";

    // Cleanup
    childCmdlist.setGraphCaptureTarget(nullptr);
    parentCmdlist.setGraphCaptureTarget(nullptr);
}

TEST(GraphTestZeCommandListGetGraphExp, GivenNestedSubgraphsThenReturnsRootParentGraph) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph rootGraph{&ctx, true, false};
    TestableGraphForCmdListQuery subgraph1{&ctx, true, false};
    TestableGraphForCmdListQuery subgraph2{&ctx, true, false};
    MockCommandList rootCmdlist;
    MockCommandList level1Cmdlist;
    MockCommandList level2Cmdlist;
    ze_graph_handle_t queryResult = {};

    // Setup root graph
    rootCmdlist.setGraphCaptureTarget(&rootGraph);

    // Create first level subgraph (child of root)
    subgraph1.setParentGraph(&rootGraph);
    subgraph1.setExecutionTarget(&level1Cmdlist);
    level1Cmdlist.setGraphCaptureTarget(&subgraph1);

    // Create second level subgraph (child of subgraph1)
    subgraph2.setParentGraph(&subgraph1);
    subgraph2.setExecutionTarget(&level2Cmdlist);
    level2Cmdlist.setGraphCaptureTarget(&subgraph2);

    // Query from level 1 subgraph - should return root graph
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetGraphExp(&level1Cmdlist, &queryResult));
    EXPECT_EQ(&rootGraph, queryResult) << "Level 1 subgraph should return root graph";
    EXPECT_NE(&subgraph1, queryResult) << "Should not return level 1 subgraph handle";

    // Query from level 2 subgraph - should also return root graph (not level 1)
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetGraphExp(&level2Cmdlist, &queryResult));
    EXPECT_EQ(&rootGraph, queryResult) << "Level 2 subgraph should return root graph, not level 1";
    EXPECT_NE(&subgraph2, queryResult) << "Should not return level 2 subgraph handle";
    EXPECT_NE(&subgraph1, queryResult) << "Should not return intermediate subgraph handle";

    // Cleanup
    level2Cmdlist.setGraphCaptureTarget(nullptr);
    level1Cmdlist.setGraphCaptureTarget(nullptr);
    rootCmdlist.setGraphCaptureTarget(nullptr);
}

TEST(GraphTestZeGraphGetPrimaryCommandListExt, GivenInvalidParametersThenReturnsAppropriateErrorCode) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    ze_command_list_handle_t queryResult = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphGetPrimaryCommandListExt(nullptr, &queryResult));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphGetPrimaryCommandListExt(&graph, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphGetPrimaryCommandListExt(&graph, &queryResult));
}

struct TestableGraphForPrimaryCommandListQuery : public L0::Graph {
    using L0::Graph::Graph;

    void setCaptureSource(L0::CommandList *src) {
        captureSrc = src;
    }

    void setPrimaryCaptureSource(L0::CommandList *src) {
        primaryCaptureSrc = src;
    }
};

TEST(GraphTestZeGraphGetPrimaryCommandListExt, GivenActiveGraphCaptureWhenQueryingPrimaryCommandListThenReturnsCaptureSource) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForPrimaryCommandListQuery graph{&ctx, true, false};
    MockCommandList cmdlist;
    ze_command_list_handle_t queryResult = nullptr;

    graph.setCaptureSource(&cmdlist);
    graph.setPrimaryCaptureSource(&cmdlist);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetPrimaryCommandListExt(&graph, &queryResult));
    EXPECT_EQ(cmdlist.toHandle(), queryResult);
}

TEST(GraphTestZeGraphGetPrimaryCommandListExt, GivenStoppedGraphCaptureWhenQueryingPrimaryCommandListThenReturnsCaptureSource) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForPrimaryCommandListQuery graph{&ctx, true, false};
    MockCommandList cmdlist;
    ze_command_list_handle_t queryResult = nullptr;

    graph.setCaptureSource(nullptr);
    graph.setPrimaryCaptureSource(&cmdlist);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetPrimaryCommandListExt(&graph, &queryResult));
    EXPECT_EQ(cmdlist.toHandle(), queryResult);
}

struct TestableGraphForVisitApi : public L0::Graph {
    using L0::Graph::Graph;

    ze_result_t visit(const ze_command_visit_ext_desc_t *desc) override {
        ++visitCalled;
        passedDesc = desc;
        return returnValue;
    }

    uint32_t visitCalled = 0;
    const ze_command_visit_ext_desc_t *passedDesc = nullptr;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
};

TEST(GraphTestZeGraphVisitExt, GivenInvalidParametersThenReturnsAppropriateErrorCode) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForVisitApi graph{&ctx, true, false};
    ze_command_visit_ext_desc_t desc{};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, L0::zeGraphVisitExt(nullptr, &desc));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, L0::zeGraphVisitExt(graph.toHandle(), nullptr));
}

TEST(GraphTestZeGraphVisitExt, GivenValidParametersThenForwardsToGraphVisitAndReturnsItsStatus) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForVisitApi graph{&ctx, true, false};
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;

    graph.returnValue = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeGraphVisitExt(graph.toHandle(), &desc));
    EXPECT_EQ(1u, graph.visitCalled);
    EXPECT_EQ(&desc, graph.passedDesc);
}

struct TestableGraphForEagerSubmit : public L0::Graph {
    using L0::Graph::Graph;

    ze_result_t visit(const ze_command_visit_ext_desc_t *desc) override {
        ++visitCalled;
        passedDesc = *desc;
        return returnValue;
    }

    uint32_t visitCalled = 0;
    ze_command_visit_ext_desc_t passedDesc{};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
};

TEST(GraphTestEagerSubmit, GivenGraphEagerSubmitEnabledWhenAppendingGraphThenSourceGraphIsVisitedWithReappendToTargetCommandList) {
    DebugManagerStateRestore restorer;
    debugManager.flags.GraphEagerSubmit.set(true);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForEagerSubmit srcGraph{&ctx, true, false};
    MockExecutableGraph execGraph;
    execGraph.src = &srcGraph;
    Mock<CommandList> cmdlist;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExt(cmdlist.toHandle(), execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(1u, srcGraph.visitCalled);
    EXPECT_EQ(ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC, srcGraph.passedDesc.stype);
    EXPECT_EQ(nullptr, srcGraph.passedDesc.pNext);
    EXPECT_EQ(ZE_COMMAND_VISIT_EXT_DEFAULT_OP_REAPPEND, srcGraph.passedDesc.defaultOp);
    EXPECT_EQ(cmdlist.toHandle(), srcGraph.passedDesc.hReappendTargetCommandList);
    EXPECT_EQ(nullptr, srcGraph.passedDesc.beforeDefaultOpClb);
    EXPECT_EQ(nullptr, srcGraph.passedDesc.afterDefaultOpClb);
    EXPECT_EQ(0u, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0u, cmdlist.appendBarrierCalled);
    EXPECT_EQ(0u, cmdlist.appendCommandListsCalled);
}

TEST(GraphTestEagerSubmit, GivenGraphEagerSubmitEnabledWhenAppendingGraphWithEventsThenWaitBeforeAndSignalAfterVisitingOnTargetCommandList) {
    DebugManagerStateRestore restorer;
    debugManager.flags.GraphEagerSubmit.set(true);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForEagerSubmit srcGraph{&ctx, true, false};
    MockExecutableGraph execGraph;
    execGraph.src = &srcGraph;
    Mock<CommandList> cmdlist;
    Mock<Event> signalEvent;
    Mock<Event> waitEvents[2];
    ze_event_handle_t waitEventsList[2] = {waitEvents[0].toHandle(), waitEvents[1].toHandle()};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExt(cmdlist.toHandle(), execGraph.toHandle(), nullptr, signalEvent.toHandle(), 2, waitEventsList));
    EXPECT_EQ(1u, srcGraph.visitCalled);
    EXPECT_EQ(1u, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(1u, cmdlist.appendBarrierCalled);
    EXPECT_EQ(0u, cmdlist.appendCommandListsCalled);
}

TEST(GraphTestEagerSubmit, GivenGraphEagerSubmitEnabledWhenWaitOnEventsFailsThenGraphIsNotVisitedAndErrorIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.GraphEagerSubmit.set(true);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForEagerSubmit srcGraph{&ctx, true, false};
    MockExecutableGraph execGraph;
    execGraph.src = &srcGraph;
    Mock<CommandList> cmdlist;
    cmdlist.appendWaitOnEventsResult = ZE_RESULT_ERROR_DEVICE_LOST;
    Mock<Event> signalEvent;
    Mock<Event> waitEvent;
    ze_event_handle_t hWaitEvent = waitEvent.toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, L0::zeCommandListAppendGraphExt(cmdlist.toHandle(), execGraph.toHandle(), nullptr, signalEvent.toHandle(), 1, &hWaitEvent));
    EXPECT_EQ(0u, srcGraph.visitCalled);
    EXPECT_EQ(0u, cmdlist.appendBarrierCalled);
}

TEST(GraphTestEagerSubmit, GivenGraphEagerSubmitEnabledWhenVisitFailsThenSignalEventIsNotAppendedAndErrorIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.GraphEagerSubmit.set(true);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForEagerSubmit srcGraph{&ctx, true, false};
    srcGraph.returnValue = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    MockExecutableGraph execGraph;
    execGraph.src = &srcGraph;
    Mock<CommandList> cmdlist;
    Mock<Event> signalEvent;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendGraphExt(cmdlist.toHandle(), execGraph.toHandle(), nullptr, signalEvent.toHandle(), 0, nullptr));
    EXPECT_EQ(1u, srcGraph.visitCalled);
    EXPECT_EQ(0u, cmdlist.appendBarrierCalled);
}

TEST(GraphTestEagerSubmit, GivenGraphEagerSubmitEnabledAndNoSourceGraphWhenAppendingGraphThenExecutableGraphIsExecuted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.GraphEagerSubmit.set(true);
    debugManager.flags.ForceGraphImplicitSyncMutation.set(0);

    GraphsCleanupGuard graphCleanup;
    MockExecutableGraph execGraph;
    Mock<CommandList> cmdlist;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExt(cmdlist.toHandle(), execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(1u, cmdlist.appendBarrierCalled); // empty executable graph is executed as barrier
}

TEST(GraphTestEagerSubmit, GivenGraphEagerSubmitDisabledWhenAppendingGraphThenSourceGraphIsNotVisitedAndExecutableGraphIsExecuted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceGraphImplicitSyncMutation.set(0);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    TestableGraphForEagerSubmit srcGraph{&ctx, true, false};
    MockExecutableGraph execGraph;
    execGraph.src = &srcGraph;
    Mock<CommandList> cmdlist;

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExt(cmdlist.toHandle(), execGraph.toHandle(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(0u, srcGraph.visitCalled);
    EXPECT_EQ(1u, cmdlist.appendBarrierCalled); // empty executable graph is executed as barrier
}

TEST(GraphTestVisit, GivenExternalCbEventsWhenReappendingThenPreserveEventsWithoutRequestingMutableCommands) {
    struct EventRecordingCommandList : Mock<CommandList> {
        void *asMutable() override {
            ++asMutableCalled;
            return nullptr;
        }

        void recordEvents(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
            signalEvents.push_back(hSignalEvent);
            auto &waits = waitEvents.emplace_back();
            if (numWaitEvents > 0) {
                waits.assign(phWaitEvents, phWaitEvents + numWaitEvents);
            }
        }

        ze_result_t appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery, ze_event_handle_t hSignalEvent,
                                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
            ++appendMetricQueryEndCalled;
            recordEvents(hSignalEvent, numWaitEvents, phWaitEvents);
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t appendSignalExternalSemaphores(uint32_t numSemaphores, const ze_external_semaphore_ext_handle_t *phSemaphores,
                                                   const ze_external_semaphore_signal_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                   uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
            ++appendSignalExternalSemaphoresCalled;
            recordEvents(hSignalEvent, numWaitEvents, phWaitEvents);
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t appendWaitExternalSemaphores(uint32_t numSemaphores, const ze_external_semaphore_ext_handle_t *phSemaphores,
                                                 const ze_external_semaphore_wait_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                 uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override {
            ++appendWaitExternalSemaphoresCalled;
            recordEvents(hSignalEvent, numWaitEvents, phWaitEvents);
            return ZE_RESULT_SUCCESS;
        }

        uint32_t asMutableCalled = 0;
        std::vector<ze_event_handle_t> signalEvents;
        std::vector<std::vector<ze_event_handle_t>> waitEvents;
    };

    Mock<Event> externalWaitEvent;
    externalWaitEvent.externalEvent = true;
    Mock<Event> regularWaitEvent;
    Mock<Event> externalSignalEvent;
    externalSignalEvent.externalEvent = true;
    ze_event_handle_t waitEvents[] = {regularWaitEvent.toHandle(), externalWaitEvent.toHandle()};
    auto hSignalEvent = externalSignalEvent.toHandle();
    auto hSemaphore = reinterpret_cast<ze_external_semaphore_ext_handle_t>(0x1234);
    ze_external_semaphore_signal_params_ext_t signalParams{};
    ze_external_semaphore_wait_params_ext_t waitParams{};

    for (bool explicitTarget : {false, true}) {
        for (uint32_t numWaitEvents : {2u, 0u}) {
            EventRecordingCommandList originalCmdList;
            EventRecordingCommandList reappendCmdList;
            auto &target = explicitTarget ? reappendCmdList : originalCmdList;
            auto &unusedTarget = explicitTarget ? originalCmdList : reappendCmdList;
            auto hOriginalCmdList = originalCmdList.toHandle();
            auto phWaitEvents = numWaitEvents ? waitEvents : nullptr;

            RecordedApiCommands commands;
            ASSERT_EQ(ZE_RESULT_SUCCESS, commands.capture<CaptureApi::zetCommandListAppendMetricQueryEnd>(hOriginalCmdList, nullptr, hSignalEvent, numWaitEvents, phWaitEvents));
            ASSERT_EQ(ZE_RESULT_SUCCESS, commands.capture<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>(hOriginalCmdList, 1u, &hSemaphore, &signalParams, hSignalEvent, numWaitEvents, phWaitEvents));
            ASSERT_EQ(ZE_RESULT_SUCCESS, commands.capture<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>(hOriginalCmdList, 1u, &hSemaphore, &waitParams, hSignalEvent, numWaitEvents, phWaitEvents));

            ze_command_visit_ext_desc_t desc{};
            desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
            desc.defaultOp = ZE_COMMAND_VISIT_EXT_DEFAULT_OP_REAPPEND;
            desc.hReappendTargetCommandList = explicitTarget ? reappendCmdList.toHandle() : nullptr;
            ASSERT_EQ(ZE_RESULT_SUCCESS, commands.visit(&desc)) << "explicitTarget: " << explicitTarget << ", numWaitEvents: " << numWaitEvents;

            EXPECT_EQ(1u, target.appendMetricQueryEndCalled);
            EXPECT_EQ(1u, target.appendSignalExternalSemaphoresCalled);
            EXPECT_EQ(1u, target.appendWaitExternalSemaphoresCalled);
            EXPECT_EQ((std::vector<ze_event_handle_t>(3, hSignalEvent)), target.signalEvents);
            ASSERT_EQ(3u, target.waitEvents.size());
            const std::vector<ze_event_handle_t> expectedWaits(waitEvents, waitEvents + numWaitEvents);
            for (const auto &waits : target.waitEvents) {
                EXPECT_EQ(expectedWaits, waits);
            }
            EXPECT_TRUE(unusedTarget.signalEvents.empty());
            EXPECT_EQ(0u, originalCmdList.asMutableCalled);
            EXPECT_EQ(0u, reappendCmdList.asMutableCalled);
        }
    }
}

TEST(GraphTestVisit, GivenInvalidPnextStypeWhenVisitingThenReturnsInvalidEnumeration) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    ze_base_desc_t invalidDesc{};
    invalidDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &invalidDesc;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorWithNullFunctionNameWhenVisitingThenReturnsInvalidNullPointer) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZE_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorWithUnknownFunctionNameWhenVisitingThenVisitorIsIgnoredAndVisitSucceeds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZE_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "unknownApiName";
    concreteVisitor.callback = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorForGetNextCommandIdWhenVisitingThenInitializationSucceeds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZE_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "zeCommandListGetNextCommandIdExp";
    concreteVisitor.callback = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorForGetNextCommandIdExtWhenVisitingThenInitializationSucceeds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZE_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "zeCommandListGetNextCommandIdExt";
    concreteVisitor.callback = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorWithLegacyStypeWhenVisitingThenInitializationSucceeds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = static_cast<ze_structure_type_t>(ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC);
    concreteVisitor.fname = "zeCommandListGetNextCommandIdExp";
    concreteVisitor.callback = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorWithLegacyStypeAndExtWhenVisitingThenInitializationSucceeds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = static_cast<ze_structure_type_t>(ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC);
    concreteVisitor.fname = "zeCommandListGetNextCommandIdExt";
    concreteVisitor.callback = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorForGetNextCommandIdWithKernelsWhenVisitingThenInitializationSucceeds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZE_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "zeCommandListGetNextCommandIdWithKernelsExp";
    concreteVisitor.callback = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.visit(&desc));
}

TEST(GraphTestVisit, GivenConcreteVisitorForGetNextCommandIdWithKernelsExtWhenVisitingThenInitializationSucceeds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();
    ze_concrete_visitor_ext_desc_t concreteVisitor{};
    concreteVisitor.stype = ZE_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC;
    concreteVisitor.fname = "zeCommandListGetNextCommandIdWithKernelsExt";
    concreteVisitor.callback = nullptr;
    ze_command_visit_ext_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC;
    desc.pNext = &concreteVisitor;

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.visit(&desc));
}

struct TestableExecutableGraphForSourceGraphQuery : public L0::ExecutableGraph {
    using L0::ExecutableGraph::src;
};

TEST(GraphTestZeExecutableGraphGetSourceGraphExt, GivenInvalidParametersThenReturnsAppropriateErrorCode) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    TestableExecutableGraphForSourceGraphQuery execGraph;
    ze_graph_handle_t queryResult = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphGetSourceGraphExt(nullptr, &queryResult));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphGetSourceGraphExt(execGraph.toHandle(), nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeExecutableGraphGetSourceGraphExt(execGraph.toHandle(), &queryResult));

    execGraph.src = &graph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeExecutableGraphGetSourceGraphExt(execGraph.toHandle(), &queryResult));
    EXPECT_EQ(&graph, queryResult);
}

TEST(GraphTestZeGraphGetIdExt, GivenInvalidParametersThenReturnsInvalidArgument) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    uint64_t graphId = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, L0::zeGraphGetIdExt(nullptr, &graphId));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, L0::zeGraphGetIdExt(graph.toHandle(), nullptr));
}

TEST(GraphTestZeGraphGetIdExt, GivenValidGraphThenReturnsNonZeroStableId) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    auto hGraph = graph.toHandle();

    uint64_t firstQuery = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetIdExt(hGraph, &firstQuery));
    EXPECT_NE(0u, firstQuery);

    uint64_t secondQuery = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetIdExt(hGraph, &secondQuery));
    EXPECT_EQ(firstQuery, secondQuery);
}

TEST(GraphTestZeGraphGetIdExt, GivenMultipleGraphsThenIdsAreUniqueAndMonotonicallyIncreasing) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph firstGraph{&ctx, true, false};
    MockGraph secondGraph{&ctx, true, false};

    uint64_t firstQuery = 0;
    uint64_t secondQuery = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetIdExt(firstGraph.toHandle(), &firstQuery));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetIdExt(secondGraph.toHandle(), &secondQuery));

    EXPECT_LT(firstQuery, secondQuery);
}

TEST(GraphTestZeGraphGetIdExt, GivenSubgraphThenReturnsRootGraphId) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph rootGraph{&ctx, true, false};
    TestableGraphForCmdListQuery subgraph1{&ctx, true, false};
    TestableGraphForCmdListQuery subgraph2{&ctx, true, false};
    subgraph1.setParentGraph(&rootGraph);
    subgraph2.setParentGraph(&subgraph1);

    uint64_t rootId = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetIdExt(rootGraph.toHandle(), &rootId));

    uint64_t subgraph1Id = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetIdExt(subgraph1.toHandle(), &subgraph1Id));
    EXPECT_EQ(rootId, subgraph1Id);

    uint64_t subgraph2Id = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetIdExt(subgraph2.toHandle(), &subgraph2Id));
    EXPECT_EQ(rootId, subgraph2Id);
}

TEST(GraphTestZeGraphSetDestructionCallbackExp, GivenInvalidParametersThenReturnsAppropriateErrorCode) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockCommandList cmdlist;
    MockGraph graph{&ctx, true, false};
    ze_module_program_exp_desc_t progExp = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC, nullptr};

    zex_mem_graph_free_callback_fn_t clb = +[](void *) {};
    uint64_t userData = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphSetDestructionCallbackExp(nullptr, clb, nullptr, nullptr)) << "Graph handle cannot be null";
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphSetDestructionCallbackExp(&graph, nullptr, nullptr, nullptr)) << "Callback cannot be null";
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeGraphSetDestructionCallbackExp(&graph, clb, nullptr, &progExp)) << "Pnext extensions are not supported";
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(&graph, clb, nullptr, nullptr)) << "User data can be null";
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(&graph, clb, &userData, nullptr)) << "Valid callback+user data is supported";
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(&graph, clb, &userData, nullptr)) << "Multiple callbacks are allowed";
}

TEST(GraphTestZeGraphSetDestructionCallbackExp, GivenValidCallbacksThenInvokesThemInOrderDuringGraphTearDown) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockCommandList cmdlist;

    using UserDataT = uint64_t;
    UserDataT userData = 0U;
    zex_mem_graph_free_callback_fn_t clbMul2 = +[](void *p) { reinterpret_cast<UserDataT *>(p)[0] *= 2; };
    zex_mem_graph_free_callback_fn_t clbAdd1 = +[](void *p) { reinterpret_cast<UserDataT *>(p)[0] += 1; };
    {
        MockGraph graph{&ctx, true, false};
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(&graph, clbAdd1, &userData, nullptr)); // 0+1
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(&graph, clbMul2, &userData, nullptr)); // 1*2
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(&graph, clbAdd1, &userData, nullptr)); // 2+1
        EXPECT_EQ(0U, userData);
    }
    EXPECT_EQ(3U, userData);
}

TEST(GraphNodeTest, GivenGraphNodeWithTypeAndFlagsWhenGettingPropertiesThenCreationTypeAndFlagsAreReturned) {
    const ze_mutable_graph_node_exp_flags_t flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_SIGNAL_EVENT;
    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, flags};

    ze_graph_node_exp_properties_t nodeProperties = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_PROPERTIES};
    nodeProperties.type = ZE_GRAPH_NODE_EXP_TYPE_FORCE_UINT32;
    nodeProperties.flags = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::GraphNode::fromHandle(node.toHandle())->getProperties(&nodeProperties));
    EXPECT_EQ(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, nodeProperties.type);
    EXPECT_EQ(flags, nodeProperties.flags);
}

TEST(GraphNodeTest, GivenGraphNodeWithoutFlagsWhenGettingPropertiesThenZeroFlagsAreReturned) {
    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U};

    ze_graph_node_exp_properties_t nodeProperties = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_PROPERTIES};
    nodeProperties.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT;
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.getProperties(&nodeProperties));
    EXPECT_EQ(0U, nodeProperties.flags);
}

TEST(GraphNodeTest, GivenGraphNodeWithGroupCountReferenceWhenUpdatingGroupCountThenReferencedGroupCountIsModifiedAndReturnedAsCurrent) {
    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT};
    ze_group_count_t capturedGroupCount = {1, 2, 3};
    node.setGroupCounts(&capturedGroupCount);

    auto currentGroupCount = node.getCurrentGroupCount();
    EXPECT_EQ(1u, currentGroupCount.groupCountX);
    EXPECT_EQ(2u, currentGroupCount.groupCountY);
    EXPECT_EQ(3u, currentGroupCount.groupCountZ);

    const ze_group_count_t newGroupCount = {4, 5, 6};
    node.updateGroupCount(newGroupCount);
    EXPECT_EQ(4u, capturedGroupCount.groupCountX);
    EXPECT_EQ(5u, capturedGroupCount.groupCountY);
    EXPECT_EQ(6u, capturedGroupCount.groupCountZ);

    currentGroupCount = node.getCurrentGroupCount();
    EXPECT_EQ(4u, currentGroupCount.groupCountX);
    EXPECT_EQ(5u, currentGroupCount.groupCountY);
    EXPECT_EQ(6u, currentGroupCount.groupCountZ);
}

TEST(GraphNodeTest, GivenGraphNodeWithGroupSizeReferenceWhenUpdatingGroupSizeThenReferencedGroupSizeIsModifiedAndReturnedAsCurrent) {
    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE};
    ze_group_size_t capturedGroupSize = {1, 2, 3};
    node.setGroupSizes(&capturedGroupSize);

    auto currentGroupSize = node.getCurrentGroupSize();
    EXPECT_EQ(1u, currentGroupSize.groupSizeX);
    EXPECT_EQ(2u, currentGroupSize.groupSizeY);
    EXPECT_EQ(3u, currentGroupSize.groupSizeZ);

    const ze_group_size_t newGroupSize = {4, 2, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateGroupSize(newGroupSize));
    EXPECT_EQ(4u, capturedGroupSize.groupSizeX);
    EXPECT_EQ(2u, capturedGroupSize.groupSizeY);
    EXPECT_EQ(1u, capturedGroupSize.groupSizeZ);

    currentGroupSize = node.getCurrentGroupSize();
    EXPECT_EQ(4u, currentGroupSize.groupSizeX);
    EXPECT_EQ(2u, currentGroupSize.groupSizeY);
    EXPECT_EQ(1u, currentGroupSize.groupSizeZ);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWithoutGroupSizeReferenceWhenUpdatingGroupSizeThenKernelGroupSizeIsModifiedAndReturnedAsCurrent) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    kernel.privateState.groupSize[0] = 1U;
    kernel.privateState.groupSize[1] = 2U;
    kernel.privateState.groupSize[2] = 3U;

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE};
    node.setKernel(&kernel);

    auto currentGroupSize = node.getCurrentGroupSize();
    EXPECT_EQ(1u, currentGroupSize.groupSizeX);
    EXPECT_EQ(2u, currentGroupSize.groupSizeY);
    EXPECT_EQ(3u, currentGroupSize.groupSizeZ);

    const ze_group_size_t newGroupSize = {4, 2, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateGroupSize(newGroupSize));
    EXPECT_EQ(4u, kernel.getGroupSize()[0]);
    EXPECT_EQ(2u, kernel.getGroupSize()[1]);
    EXPECT_EQ(1u, kernel.getGroupSize()[2]);

    currentGroupSize = node.getCurrentGroupSize();
    EXPECT_EQ(4u, currentGroupSize.groupSizeX);
    EXPECT_EQ(2u, currentGroupSize.groupSizeY);
    EXPECT_EQ(1u, currentGroupSize.groupSizeZ);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWithStoredKernelArgumentsWhenUpdatingKernelArgumentsThenStoredValuesAreModifiedAndReturnedAsCurrent) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    auto &explicitArgs = kernel.descriptor.payloadMappings.explicitArgs;
    explicitArgs.resize(3);
    explicitArgs[0] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    explicitArgs[0].getTraits().argByValSize = sizeof(uint32_t);
    explicitArgs[1] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTPointer};
    explicitArgs[1].getTraits().argByValSize = sizeof(void *);
    explicitArgs[1].getTraits().addressQualifier = NEO::KernelArgMetadata::AddrGlobal;
    explicitArgs[2] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTPointer};
    explicitArgs[2].getTraits().argByValSize = sizeof(void *);
    explicitArgs[2].getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;

    uint32_t capturedValue = 1U;
    void *capturedPointer = reinterpret_cast<void *>(0x1000);
    size_t capturedSlmSize = 64U;
    std::array<void *, 3> capturedArguments = {&capturedValue, &capturedPointer, &capturedSlmSize};

    L0::ClosureExternalStorage storage;
    auto argumentsId = storage.registerKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), capturedArguments.data());

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT};
    node.setKernel(&kernel);
    node.setExternalStorage(&storage);
    node.setArgumentsId(argumentsId);

    auto currentArguments = node.getCurrentKernelArguments();
    ASSERT_NE(nullptr, currentArguments[0].argValue);
    EXPECT_EQ(capturedValue, *static_cast<const uint32_t *>(currentArguments[0].argValue));
    EXPECT_EQ(sizeof(uint32_t), currentArguments[0].argSize);
    ASSERT_NE(nullptr, currentArguments[1].argValue);
    EXPECT_EQ(capturedPointer, *static_cast<void *const *>(currentArguments[1].argValue));
    EXPECT_EQ(sizeof(void *), currentArguments[1].argSize);
    EXPECT_EQ(nullptr, currentArguments[2].argValue);
    EXPECT_EQ(capturedSlmSize, currentArguments[2].argSize);

    const uint32_t newValue = 2U;
    void *const newPointer = reinterpret_cast<void *>(0x2000);
    const size_t newSlmSize = 128U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateKernelArgument(node.getStoredKernelArguments(), &newValue, sizeof(newValue), 0U));
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateKernelArgument(node.getStoredKernelArguments(), &newPointer, sizeof(newPointer), 1U));
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateKernelArgument(node.getStoredKernelArguments(), nullptr, newSlmSize, 2U));

    currentArguments = node.getCurrentKernelArguments();
    ASSERT_NE(nullptr, currentArguments[0].argValue);
    EXPECT_EQ(newValue, *static_cast<const uint32_t *>(currentArguments[0].argValue));
    EXPECT_EQ(sizeof(uint32_t), currentArguments[0].argSize);
    ASSERT_NE(nullptr, currentArguments[1].argValue);
    EXPECT_EQ(newPointer, *static_cast<void *const *>(currentArguments[1].argValue));
    EXPECT_EQ(sizeof(void *), currentArguments[1].argSize);
    EXPECT_EQ(nullptr, currentArguments[2].argValue);
    EXPECT_EQ(newSlmSize, currentArguments[2].argSize);

    auto storedArguments = storage.getKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), argumentsId);
    EXPECT_EQ(newValue, *static_cast<uint32_t *>(storedArguments[0]));
    EXPECT_EQ(newPointer, *static_cast<void **>(storedArguments[1]));
    EXPECT_EQ(newSlmSize, *static_cast<size_t *>(storedArguments[2]));

    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateKernelArgument(node.getStoredKernelArguments(), nullptr, sizeof(void *), 1U));
    currentArguments = node.getCurrentKernelArguments();
    ASSERT_NE(nullptr, currentArguments[1].argValue);
    EXPECT_EQ(nullptr, *static_cast<void *const *>(currentArguments[1].argValue));
    EXPECT_EQ(sizeof(void *), currentArguments[1].argSize);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWithStoredKernelArgumentsWhenUpdatingValueArgumentWithLargerSizeThenValueIsTruncatedToArgumentSize) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    auto &explicitArgs = kernel.descriptor.payloadMappings.explicitArgs;
    explicitArgs.resize(1);
    explicitArgs[0] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    explicitArgs[0].getTraits().argByValSize = sizeof(uint32_t);
    explicitArgs[0].as<NEO::ArgDescValue>().elements.push_back(NEO::ArgDescValue::Element{0U, sizeof(uint32_t), 0U});

    uint32_t capturedValue = 1U;
    std::array<void *, 1> capturedArguments = {&capturedValue};
    L0::ClosureExternalStorage storage;
    auto argumentsId = storage.registerKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), capturedArguments.data());

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT};
    node.setKernel(&kernel);
    node.setExternalStorage(&storage);
    node.setArgumentsId(argumentsId);

    const std::array<uint32_t, 2> largerValue = {2U, 3U};
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateKernelArgument(node.getStoredKernelArguments(), largerValue.data(), sizeof(largerValue), 0U));

    auto storedArguments = storage.getKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), argumentsId);
    EXPECT_EQ(largerValue[0], *static_cast<uint32_t *>(storedArguments[0]));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWithStoredKernelArgumentsWhenUpdatingValueArgumentWithSizeNotCoveringAllElementsThenInvalidArgumentIsReturnedAndValueIsNotChanged) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    auto &explicitArgs = kernel.descriptor.payloadMappings.explicitArgs;
    explicitArgs.resize(1);
    explicitArgs[0] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    explicitArgs[0].getTraits().argByValSize = 2 * sizeof(uint32_t);
    explicitArgs[0].as<NEO::ArgDescValue>().elements.push_back(NEO::ArgDescValue::Element{0U, sizeof(uint32_t), 0U});
    explicitArgs[0].as<NEO::ArgDescValue>().elements.push_back(NEO::ArgDescValue::Element{sizeof(uint32_t), sizeof(uint32_t), sizeof(uint32_t)});

    std::array<uint32_t, 2> capturedValue = {1U, 2U};
    std::array<void *, 1> capturedArguments = {capturedValue.data()};
    L0::ClosureExternalStorage storage;
    auto argumentsId = storage.registerKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), capturedArguments.data());

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT};
    node.setKernel(&kernel);
    node.setExternalStorage(&storage);
    node.setArgumentsId(argumentsId);

    const uint32_t smallerValue = 3U;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, node.updateKernelArgument(node.getStoredKernelArguments(), &smallerValue, sizeof(smallerValue), 0U));

    auto storedArguments = storage.getKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), argumentsId);
    EXPECT_EQ(0, memcmp(capturedValue.data(), storedArguments[0], sizeof(capturedValue)));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWithStoredKernelArgumentsWhenUpdatingArgumentWithIndexOutOfRangeThenInvalidArgumentIsReturned) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    auto &explicitArgs = kernel.descriptor.payloadMappings.explicitArgs;
    explicitArgs.resize(1);
    explicitArgs[0] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    explicitArgs[0].getTraits().argByValSize = sizeof(uint32_t);

    uint32_t capturedValue = 1U;
    std::array<void *, 1> capturedArguments = {&capturedValue};
    L0::ClosureExternalStorage storage;
    auto argumentsId = storage.registerKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), capturedArguments.data());

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT};
    node.setKernel(&kernel);
    node.setExternalStorage(&storage);
    node.setArgumentsId(argumentsId);

    const uint32_t newValue = 2U;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, node.updateKernelArgument(node.getStoredKernelArguments(), &newValue, sizeof(newValue), 1U));
}

using GraphFlatCaptureKernelArgumentTest = Test<ModuleImmutableDataFixture>;

TEST_F(GraphFlatCaptureKernelArgumentTest, GivenFlatCapturedAppendKernelWithArgumentsWhenUpdatingSlmAndNullBufferArgumentsByMutableCommandIdThenKernelStateAndStoredArgumentsAreUpdated) {
    GraphsCleanupGuard graphCleanup;
    constexpr uint32_t perHwThreadPrivateMemorySize = 0u;
    auto mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySize);
    createModuleFromMockBinary(perHwThreadPrivateMemorySize, false, mockKernelImmData.get());
    mockKernelImmData->resizeExplicitArgs(2);
    auto &explicitArgs = mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs;
    explicitArgs[0].as<NEO::ArgDescPointer>(true);
    explicitArgs[0].getTraits().argByValSize = sizeof(void *);
    explicitArgs[0].getTraits().addressQualifier = NEO::KernelArgMetadata::AddrGlobal;
    auto &slmArg = explicitArgs[1].as<NEO::ArgDescPointer>(true);
    slmArg.slmOffset = 0U;
    slmArg.requiredSlmAlignment = 16U;
    explicitArgs[1].getTraits().argByValSize = sizeof(void *);
    explicitArgs[1].getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;
    auto kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = kernelName.c_str();
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->initialize(&kernelDesc));
    kernel->setCrossThreadData(sizeof(uint32_t));

    void *capturedBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceMemDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceMemDesc, MemoryConstants::pageSize, 1u, &capturedBuffer));
    size_t capturedSlmSize = 64U;
    std::array<void *, 2> capturedArguments = {&capturedBuffer, &capturedSlmSize};

    Mock<CommandList> cmdlist;
    RecordedApiCommands flatCapture;
    ze_mutable_command_id_exp_desc_t commandIdDesc{};
    uint64_t commandId = 1U;
    ASSERT_EQ(ZE_RESULT_SUCCESS, flatCapture.capture<CaptureApi::zeCommandListGetNextCommandIdExp>(cmdlist.toHandle(), &commandIdDesc, &commandId));
    ASSERT_EQ(ZE_RESULT_SUCCESS, flatCapture.capture<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>(cmdlist.toHandle(), kernel->toHandle(), ze_group_count_t{1, 1, 1}, ze_group_size_t{1, 1, 1}, capturedArguments.data(), nullptr, nullptr, 0U, nullptr));

    const size_t newSlmSize = 128U;
    flatCapture.updateKernelArgument(commandId, 0U, sizeof(void *), nullptr);
    flatCapture.updateKernelArgument(commandId, 1U, newSlmSize, nullptr);

    const auto &closure = std::get<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>>(flatCapture.commands[1]);
    auto storedArguments = flatCapture.externalStorage.getKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), closure.indirectArgs.argumentsId);
    EXPECT_EQ(nullptr, *static_cast<void **>(storedArguments[0]));
    EXPECT_EQ(newSlmSize, *static_cast<size_t *>(storedArguments[1]));

    std::vector<uint8_t> immediateValueBuffer;
    auto bufferArgument = closure.indirectArgs.capturedKernel->getArgumentValue(0U, immediateValueBuffer);
    ASSERT_NE(nullptr, bufferArgument.argValue);
    EXPECT_EQ(nullptr, *static_cast<const void *const *>(bufferArgument.argValue));
    auto slmArgument = closure.indirectArgs.capturedKernel->getArgumentValue(1U, immediateValueBuffer);
    EXPECT_EQ(newSlmSize, slmArgument.argSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(capturedBuffer));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWithoutStoredKernelArgumentsWhenUpdatingKernelArgumentThenKernelArgumentIsSet) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    kernel.checkPassedArgumentValues = true;
    kernel.passedArgumentValues.resize(1);

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT};
    node.setKernel(&kernel);

    const uint32_t newValue = 2U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateKernelArgument(node.getStoredKernelArguments(), &newValue, sizeof(newValue), 0U));
    ASSERT_EQ(sizeof(newValue), kernel.passedArgumentValues[0].size());
    EXPECT_EQ(newValue, *reinterpret_cast<const uint32_t *>(kernel.passedArgumentValues[0].data()));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWithoutStoredKernelArgumentsWhenGettingCurrentKernelArgumentsThenValuesAreReturnedFromKernel) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    auto &explicitArgs = kernel.descriptor.payloadMappings.explicitArgs;
    explicitArgs.resize(3);
    explicitArgs[0] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTPointer};
    explicitArgs[0].getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;
    explicitArgs[1] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    explicitArgs[1].getTraits().argByValSize = 2 * sizeof(uint32_t);
    auto &firstImmediateElements = explicitArgs[1].as<NEO::ArgDescValue>().elements;
    firstImmediateElements.push_back({.offset = 20U, .size = sizeof(uint32_t), .sourceOffset = 0U});
    firstImmediateElements.push_back({.offset = 8U, .size = sizeof(uint32_t), .sourceOffset = sizeof(uint32_t)});
    explicitArgs[2] = NEO::ArgDescriptor{NEO::ArgDescriptor::argTValue};
    explicitArgs[2].getTraits().argByValSize = 2 * sizeof(uint32_t);
    auto &secondImmediateElements = explicitArgs[2].as<NEO::ArgDescValue>().elements;
    secondImmediateElements.push_back({.offset = 32U, .size = sizeof(uint32_t), .sourceOffset = 0U});
    secondImmediateElements.push_back({.offset = 24U, .size = sizeof(uint32_t), .sourceOffset = sizeof(uint32_t)});
    kernel.privateState.slmArgSizes.resize(3);
    kernel.privateState.slmArgSizes[0] = 64U;
    const uint32_t firstImmediateParts[] = {0x11223344U, 0x55667788U};
    const uint32_t secondImmediateParts[] = {0x99aabbccU, 0xddeeff00U};
    memcpy_s(&kernel.privateState.crossThreadData[20], sizeof(uint32_t), &firstImmediateParts[0], sizeof(uint32_t));
    memcpy_s(&kernel.privateState.crossThreadData[8], sizeof(uint32_t), &firstImmediateParts[1], sizeof(uint32_t));
    memcpy_s(&kernel.privateState.crossThreadData[32], sizeof(uint32_t), &secondImmediateParts[0], sizeof(uint32_t));
    memcpy_s(&kernel.privateState.crossThreadData[24], sizeof(uint32_t), &secondImmediateParts[1], sizeof(uint32_t));

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT};
    node.setKernel(&kernel);

    const auto currentArguments = node.getCurrentKernelArguments();
    const auto &slmArgument = currentArguments[0];
    EXPECT_EQ(nullptr, slmArgument.argValue);
    EXPECT_EQ(64U, slmArgument.argSize);

    const auto &firstImmediateArgument = currentArguments[1];
    const auto &secondImmediateArgument = currentArguments[2];
    ASSERT_NE(nullptr, firstImmediateArgument.argValue);
    ASSERT_NE(nullptr, secondImmediateArgument.argValue);
    EXPECT_NE(firstImmediateArgument.argValue, secondImmediateArgument.argValue);
    EXPECT_EQ(2 * sizeof(uint32_t), firstImmediateArgument.argSize);
    EXPECT_EQ(2 * sizeof(uint32_t), secondImmediateArgument.argSize);
    const auto firstImmediateValue = static_cast<const uint32_t *>(firstImmediateArgument.argValue);
    EXPECT_EQ(firstImmediateParts[0], firstImmediateValue[0]);
    EXPECT_EQ(firstImmediateParts[1], firstImmediateValue[1]);
    const auto secondImmediateValue = static_cast<const uint32_t *>(secondImmediateArgument.argValue);
    EXPECT_EQ(secondImmediateParts[0], secondImmediateValue[0]);
    EXPECT_EQ(secondImmediateParts[1], secondImmediateValue[1]);

    const auto repeatedArguments = node.getCurrentKernelArguments();
    EXPECT_EQ(firstImmediateArgument.argValue, repeatedArguments[1].argValue);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGraphNodeWhenUpdatingGlobalOffsetThenKernelGlobalOffsetIsSetAndReturnedAsCurrent) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    kernel.setGlobalOffsetExp(1U, 2U, 3U);

    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET};
    node.setKernel(&kernel);

    auto currentGlobalOffset = node.getCurrentGlobalOffset();
    EXPECT_EQ(1U, currentGlobalOffset[0]);
    EXPECT_EQ(2U, currentGlobalOffset[1]);
    EXPECT_EQ(3U, currentGlobalOffset[2]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, node.updateGlobalOffset(4U, 5U, 6U));
    EXPECT_EQ(4U, kernel.privateState.globalOffsets[0]);
    EXPECT_EQ(5U, kernel.privateState.globalOffsets[1]);
    EXPECT_EQ(6U, kernel.privateState.globalOffsets[2]);

    currentGlobalOffset = node.getCurrentGlobalOffset();
    EXPECT_EQ(4U, currentGlobalOffset[0]);
    EXPECT_EQ(5U, currentGlobalOffset[1]);
    EXPECT_EQ(6U, currentGlobalOffset[2]);
}

TEST(GraphNodeTest, GivenInvalidParametersWhenGettingNodePropertiesThroughApiThenInvalidArgumentIsReturned) {
    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U};
    ze_graph_node_exp_properties_t nodeProperties = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_PROPERTIES};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodeGetPropertiesExp(nullptr, &nodeProperties));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodeGetPropertiesExp(node.toHandle(), nullptr));
}

TEST(GraphNodeTest, GivenGraphNodeWhenGettingNodePropertiesThroughApiThenCreationTypeAndFlagsAreReturned) {
    const ze_mutable_graph_node_exp_flags_t flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE;
    L0::GraphNode node{ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, flags};

    ze_graph_node_exp_properties_t nodeProperties = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_PROPERTIES};
    nodeProperties.type = ZE_GRAPH_NODE_EXP_TYPE_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodeGetPropertiesExp(node.toHandle(), &nodeProperties));
    EXPECT_EQ(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, nodeProperties.type);
    EXPECT_EQ(flags, nodeProperties.flags);
}

TEST(GraphGetNodesTest, GivenInvalidParametersWhenGettingNodesThroughApiThenInvalidArgumentIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.stopCapturing();

    uint32_t graphNodesCount = 0U;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphGetNodesExp(nullptr, &graphNodesCount, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphGetNodesExp(graph.toHandle(), nullptr, nullptr));
}

TEST(GraphGetNodesTest, GivenMutableGraphWithNodesWhenGettingNodesThroughApiThenNodesAreReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U));
    graph.stopCapturing();

    uint32_t graphNodesCount = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetNodesExp(graph.toHandle(), &graphNodesCount, nullptr));
    ASSERT_EQ(1u, graphNodesCount);

    ze_graph_node_handle_t graphNodeHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphGetNodesExp(graph.toHandle(), &graphNodesCount, &graphNodeHandle));
    EXPECT_EQ((*graph.graphNodes)[0]->toHandle(), graphNodeHandle);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenNodesWithAndWithoutStoredKernelArgumentsWhenUpdatingWithKernelArgumentAndLaunchKernelDescriptorsThenStoredArgumentOrKernelArgumentIsUpdated) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> argumentsKernel;
    argumentsKernel.setModule(&module);
    Mock<KernelImp> parametersKernel;
    parametersKernel.setModule(&module);
    for (auto *kernel : {&argumentsKernel, &parametersKernel}) {
        setUint32ValueArguments(*kernel, 1U);
    }

    uint32_t capturedValue = 1U;
    std::array<void *, 1> capturedArguments = {&capturedValue};
    auto &argumentsExplicitArgs = argumentsKernel.descriptor.payloadMappings.explicitArgs;

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    auto &storage = graph.recordedApiCommands.externalStorage;
    auto argumentsId = storage.registerKernelArguments(std::span(argumentsExplicitArgs.begin(), argumentsExplicitArgs.end()), capturedArguments.data());
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.stopCapturing();

    auto &argumentsNode = *(*graph.graphNodes)[0];
    argumentsNode.setKernel(&argumentsKernel);
    argumentsNode.setExternalStorage(&storage);
    argumentsNode.setArgumentsId(argumentsId);
    auto &parametersNode = *(*graph.graphNodes)[1];
    parametersNode.setKernel(&parametersKernel);
    std::array<ze_graph_node_handle_t, 2> hNodes = {argumentsNode.toHandle(), parametersNode.toHandle()};

    const uint32_t kernelArgumentValue = 2U;
    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    kernelArgumentDesc.argIndex = 0U;
    kernelArgumentDesc.argSize = sizeof(kernelArgumentValue);
    kernelArgumentDesc.pArgValue = &kernelArgumentValue;
    std::array<const void *, 2> updateDescs = {&kernelArgumentDesc, &kernelArgumentDesc};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 2, hNodes.data(), updateDescs.data(), nullptr));

    auto storedArguments = storage.getKernelArguments(std::span(argumentsExplicitArgs.begin(), argumentsExplicitArgs.end()), argumentsId);
    EXPECT_EQ(kernelArgumentValue, *static_cast<uint32_t *>(storedArguments[0]));
    ASSERT_EQ(sizeof(kernelArgumentValue), parametersKernel.passedArgumentValues[0].size());
    EXPECT_EQ(kernelArgumentValue, *reinterpret_cast<const uint32_t *>(parametersKernel.passedArgumentValues[0].data()));

    const uint32_t launchKernelArgumentValue = 3U;
    const void *launchKernelArgValues[] = {&launchKernelArgumentValue};
    const size_t launchKernelArgSizes[] = {sizeof(launchKernelArgumentValue)};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = launchKernelArgSizes;
    launchKernelDesc.pArgValues = launchKernelArgValues;
    updateDescs = {&launchKernelDesc, &launchKernelDesc};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 2, hNodes.data(), updateDescs.data(), nullptr));

    EXPECT_EQ(launchKernelArgumentValue, *static_cast<uint32_t *>(storedArguments[0]));
    ASSERT_EQ(sizeof(launchKernelArgumentValue), parametersKernel.passedArgumentValues[0].size());
    EXPECT_EQ(launchKernelArgumentValue, *reinterpret_cast<const uint32_t *>(parametersKernel.passedArgumentValues[0].data()));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenKernelArgumentDescriptorForNodeWithoutKernelArgumentFlagOrWithInvalidIndexWhenUpdatingNodeThenInvalidArgumentIsReturnedAndArgumentIsNotUpdated) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    setUint32ValueArguments(kernel, 1U);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.stopCapturing();
    (*graph.graphNodes)[0]->setKernel(&kernel);
    (*graph.graphNodes)[1]->setKernel(&kernel);

    const uint32_t kernelArgumentValue = 2U;
    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    kernelArgumentDesc.argIndex = 0U;
    kernelArgumentDesc.argSize = sizeof(kernelArgumentValue);
    kernelArgumentDesc.pArgValue = &kernelArgumentValue;
    const void *updateDesc = &kernelArgumentDesc;

    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));

    kernelArgumentDesc.argIndex = 1U;
    hNode = (*graph.graphNodes)[1]->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));

    EXPECT_TRUE(kernel.passedArgumentValues[0].empty());
}

TEST_F(GraphTestApiCaptureWithDevice, GivenLaunchKernelDescriptorWithoutArgumentArraysWhenUpdatingNodeWithKernelArgumentFlagThenInvalidArgumentIsReturnedOnlyForKernelWithArguments) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernelWithArguments;
    kernelWithArguments.setModule(&module);
    setUint32ValueArguments(kernelWithArguments, 1U);
    Mock<KernelImp> kernelWithoutArguments;
    kernelWithoutArguments.setModule(&module);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.stopCapturing();
    (*graph.graphNodes)[0]->setKernel(&kernelWithArguments);
    (*graph.graphNodes)[1]->setKernel(&kernelWithoutArguments);

    const uint32_t kernelArgumentValue = 2U;
    const void *argValues[] = {&kernelArgumentValue};
    const size_t argSizes[] = {sizeof(kernelArgumentValue)};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    const void *updateDesc = &launchKernelDesc;

    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    launchKernelDesc.pArgValues = argValues;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    launchKernelDesc.pArgValues = nullptr;
    launchKernelDesc.pArgSizes = argSizes;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));

    launchKernelDesc.pArgSizes = nullptr;
    hNode = (*graph.graphNodes)[1]->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenKernelWithTwoArgumentsOfDifferentSizesWhenUpdatingNodesWithLaunchKernelDescriptorThenEachArgumentIsUpdated) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> argumentsKernel;
    argumentsKernel.setModule(&module);
    Mock<KernelImp> parametersKernel;
    parametersKernel.setModule(&module);
    for (auto *kernel : {&argumentsKernel, &parametersKernel}) {
        setUint32ValueArguments(*kernel, 2U);
        auto &secondArgument = kernel->descriptor.payloadMappings.explicitArgs[1];
        secondArgument.getTraits().argByValSize = sizeof(uint64_t);
        secondArgument.as<NEO::ArgDescValue>().elements[0].size = sizeof(uint64_t);
    }

    uint32_t capturedFirstValue = 1U;
    uint64_t capturedSecondValue = 2U;
    std::array<void *, 2> capturedArguments = {&capturedFirstValue, &capturedSecondValue};
    auto &argumentsExplicitArgs = argumentsKernel.descriptor.payloadMappings.explicitArgs;

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    auto &storage = graph.recordedApiCommands.externalStorage;
    auto argumentsId = storage.registerKernelArguments(std::span(argumentsExplicitArgs.begin(), argumentsExplicitArgs.end()), capturedArguments.data());
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.stopCapturing();

    auto &argumentsNode = *(*graph.graphNodes)[0];
    argumentsNode.setKernel(&argumentsKernel);
    argumentsNode.setExternalStorage(&storage);
    argumentsNode.setArgumentsId(argumentsId);
    auto &parametersNode = *(*graph.graphNodes)[1];
    parametersNode.setKernel(&parametersKernel);
    std::array<ze_graph_node_handle_t, 2> hNodes = {argumentsNode.toHandle(), parametersNode.toHandle()};

    const uint32_t firstArgumentValue = 3U;
    const uint64_t secondArgumentValue = 0x100000004U;
    const void *argValues[] = {&firstArgumentValue, &secondArgumentValue};
    const size_t argSizes[] = {sizeof(firstArgumentValue), sizeof(secondArgumentValue)};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = argSizes;
    launchKernelDesc.pArgValues = argValues;
    std::array<const void *, 2> updateDescs = {&launchKernelDesc, &launchKernelDesc};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 2, hNodes.data(), updateDescs.data(), nullptr));

    auto storedArguments = storage.getKernelArguments(std::span(argumentsExplicitArgs.begin(), argumentsExplicitArgs.end()), argumentsId);
    EXPECT_EQ(firstArgumentValue, *static_cast<uint32_t *>(storedArguments[0]));
    EXPECT_EQ(secondArgumentValue, *static_cast<uint64_t *>(storedArguments[1]));
    ASSERT_EQ(sizeof(firstArgumentValue), parametersKernel.passedArgumentValues[0].size());
    EXPECT_EQ(firstArgumentValue, *reinterpret_cast<const uint32_t *>(parametersKernel.passedArgumentValues[0].data()));
    ASSERT_EQ(sizeof(secondArgumentValue), parametersKernel.passedArgumentValues[1].size());
    EXPECT_EQ(secondArgumentValue, *reinterpret_cast<const uint64_t *>(parametersKernel.passedArgumentValues[1].data()));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenArgumentValueTooSmallForSecondNodeWhenUpdatingNodesThenInvalidArgumentIsReturnedAndFirstNodeIsNotUpdated) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> firstKernel;
    firstKernel.setModule(&module);
    setUint32ValueArguments(firstKernel, 1U);
    Mock<KernelImp> secondKernel;
    secondKernel.setModule(&module);
    setUint32ValueArguments(secondKernel, 1U);
    auto &secondKernelArgument = secondKernel.descriptor.payloadMappings.explicitArgs[0];
    secondKernelArgument.getTraits().argByValSize = sizeof(uint64_t);
    secondKernelArgument.as<NEO::ArgDescValue>().elements.push_back({.size = sizeof(uint32_t), .sourceOffset = sizeof(uint32_t)});

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.stopCapturing();
    (*graph.graphNodes)[0]->setKernel(&firstKernel);
    (*graph.graphNodes)[1]->setKernel(&secondKernel);
    std::array<ze_graph_node_handle_t, 2> hNodes = {(*graph.graphNodes)[0]->toHandle(), (*graph.graphNodes)[1]->toHandle()};

    const uint32_t kernelArgumentValue = 2U;
    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    kernelArgumentDesc.argIndex = 0U;
    kernelArgumentDesc.argSize = sizeof(kernelArgumentValue);
    kernelArgumentDesc.pArgValue = &kernelArgumentValue;
    const void *argValues[] = {&kernelArgumentValue};
    const size_t argSizes[] = {sizeof(kernelArgumentValue)};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = argSizes;
    launchKernelDesc.pArgValues = argValues;

    for (const void *updateDesc : {static_cast<const void *>(&kernelArgumentDesc), static_cast<const void *>(&launchKernelDesc)}) {
        std::array<const void *, 2> updateDescs = {updateDesc, updateDesc};
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 2, hNodes.data(), updateDescs.data(), nullptr));
    }

    EXPECT_TRUE(firstKernel.passedArgumentValues[0].empty());
    EXPECT_TRUE(secondKernel.passedArgumentValues[0].empty());
}

TEST_F(GraphTestApiCaptureWithDevice, GivenNodeWithDefaultFlagsWhenUpdatingNodeWithLaunchKernelDescriptorThenArgumentsAreUpdatedOnlyWhenArgumentArraysArePresent) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    setUint32ValueArguments(kernel, 1U);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U));
    graph.stopCapturing();
    auto &node = *(*graph.graphNodes)[0];
    node.setKernel(&kernel);
    ze_group_count_t capturedGroupCount = {1, 1, 1};
    ze_group_size_t capturedGroupSize = {1, 1, 1};
    node.setGroupCounts(&capturedGroupCount);
    node.setGroupSizes(&capturedGroupSize);

    const uint32_t kernelArgumentValue = 2U;
    const void *argValues[] = {&kernelArgumentValue};
    const size_t argSizes[] = {sizeof(kernelArgumentValue)};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupCount = {2, 3, 4};
    launchKernelDesc.groupSize = {8, 4, 2};
    const void *updateDesc = &launchKernelDesc;
    ze_graph_node_handle_t hNode = node.toHandle();

    launchKernelDesc.pArgValues = argValues;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    launchKernelDesc.pArgValues = nullptr;
    launchKernelDesc.pArgSizes = argSizes;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    EXPECT_TRUE(kernel.passedArgumentValues[0].empty());
    EXPECT_EQ(1u, capturedGroupCount.groupCountX);
    EXPECT_EQ(1u, capturedGroupSize.groupSizeX);

    launchKernelDesc.pArgValues = argValues;
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    ASSERT_EQ(sizeof(kernelArgumentValue), kernel.passedArgumentValues[0].size());
    EXPECT_EQ(kernelArgumentValue, *reinterpret_cast<const uint32_t *>(kernel.passedArgumentValues[0].data()));
    EXPECT_EQ(2u, capturedGroupCount.groupCountX);
    EXPECT_EQ(8u, capturedGroupSize.groupSizeX);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenKernelArgumentRejectedByKernelWhenUpdatingNodeThenKernelErrorIsReturned) {
    struct ArgumentRejectingKernel : public Mock<KernelImp> {
        ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) override {
            return ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE;
        }
    };

    Mock<Module> module(this->device, nullptr);
    ArgumentRejectingKernel kernel;
    kernel.setModule(&module);
    setUint32ValueArguments(kernel, 1U);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));
    graph.stopCapturing();
    (*graph.graphNodes)[0]->setKernel(&kernel);

    const uint32_t kernelArgumentValue = 2U;
    ze_graph_node_update_kernel_argument_exp_desc_t kernelArgumentDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_KERNEL_ARGUMENT_EXP_DESC};
    kernelArgumentDesc.argIndex = 0U;
    kernelArgumentDesc.argSize = sizeof(kernelArgumentValue);
    kernelArgumentDesc.pArgValue = &kernelArgumentValue;
    const void *argValues[] = {&kernelArgumentValue};
    const size_t argSizes[] = {sizeof(kernelArgumentValue)};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.pArgSizes = argSizes;
    launchKernelDesc.pArgValues = argValues;

    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    for (const void *updateDesc : {static_cast<const void *>(&kernelArgumentDesc), static_cast<const void *>(&launchKernelDesc)}) {
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    }
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGlobalOffsetDescriptorForNodeWithoutGlobalOffsetFlagWhenUpdatingNodeThenInvalidArgumentIsReturnedAndGlobalOffsetIsNotUpdated) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    kernel.setGlobalOffsetExp(1U, 2U, 3U);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));
    graph.stopCapturing();
    (*graph.graphNodes)[0]->setKernel(&kernel);

    ze_graph_node_update_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};
    globalOffsetDesc.offsetX = 4U;
    globalOffsetDesc.offsetY = 5U;
    globalOffsetDesc.offsetZ = 6U;
    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    const void *updateDesc = &globalOffsetDesc;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));

    EXPECT_EQ(1U, kernel.getGlobalOffsets()[0]);
    EXPECT_EQ(2U, kernel.getGlobalOffsets()[1]);
    EXPECT_EQ(3U, kernel.getGlobalOffsets()[2]);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGlobalOffsetRejectedByKernelWhenUpdatingNodeThenKernelErrorIsReturned) {
    struct KernelRejectingGlobalOffset : Mock<KernelImp> {
        ze_result_t setGlobalOffsetExp(uint32_t, uint32_t, uint32_t) override {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    };
    Mock<Module> module(this->device, nullptr);
    KernelRejectingGlobalOffset kernel;
    kernel.setModule(&module);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET));
    graph.stopCapturing();
    (*graph.graphNodes)[0]->setKernel(&kernel);

    ze_graph_node_update_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};
    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    const void *updateDesc = &globalOffsetDesc;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
}

TEST_F(GraphTestApiCaptureWithDevice, GivenGroupSizeRejectedByKernelWhenUpdatingNodeThenKernelErrorIsReturned) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);

    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE));
    graph.stopCapturing();
    (*graph.graphNodes)[0]->setKernel(&kernel);

    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = {0, 1, 1};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupSize = {0, 1, 1};

    ze_graph_node_handle_t hNode = (*graph.graphNodes)[0]->toHandle();
    for (const void *updateDesc : {static_cast<const void *>(&groupSizeDesc), static_cast<const void *>(&launchKernelDesc)}) {
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphNodesUpdateExp(graph.toHandle(), 1, &hNode, &updateDesc, nullptr));
    }
}

TEST_F(GraphTestApiCaptureWithDevice, GivenNodesWithAndWithoutGroupSizeReferenceWhenUpdatingGroupSizeThenReferencedGroupSizeOrKernelGroupSizeIsUpdated) {
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> parametersKernel;
    parametersKernel.setModule(&module);
    Mock<KernelImp> argumentsKernel;
    argumentsKernel.setModule(&module);

    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = {4, 2, 1};
    ze_graph_node_update_launch_kernel_exp_desc_t launchKernelDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_LAUNCH_KERNEL_EXP_DESC};
    launchKernelDesc.groupSize = {4, 2, 1};

    for (const void *updateDesc : {static_cast<const void *>(&groupSizeDesc), static_cast<const void *>(&launchKernelDesc)}) {
        GraphsCleanupGuard graphCleanup;
        ContextStubMock ctx;
        MockGraph graph{&ctx, true, true};
        graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE));
        graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE));
        graph.stopCapturing();

        parametersKernel.privateState.groupSize[0] = 1U;
        parametersKernel.privateState.groupSize[1] = 1U;
        parametersKernel.privateState.groupSize[2] = 1U;
        argumentsKernel.privateState.groupSize[0] = 1U;
        argumentsKernel.privateState.groupSize[1] = 1U;
        argumentsKernel.privateState.groupSize[2] = 1U;
        ze_group_size_t capturedGroupSize = {1, 1, 1};

        auto &parametersNode = *(*graph.graphNodes)[0];
        parametersNode.setKernel(&parametersKernel);
        auto &argumentsNode = *(*graph.graphNodes)[1];
        argumentsNode.setKernel(&argumentsKernel);
        argumentsNode.setGroupSizes(&capturedGroupSize);

        std::array<ze_graph_node_handle_t, 2> hNodes = {parametersNode.toHandle(), argumentsNode.toHandle()};
        std::array<const void *, 2> updateDescs = {updateDesc, updateDesc};
        EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(graph.toHandle(), 2, hNodes.data(), updateDescs.data(), nullptr));

        EXPECT_EQ(4u, parametersKernel.getGroupSize()[0]);
        EXPECT_EQ(2u, parametersKernel.getGroupSize()[1]);
        EXPECT_EQ(1u, parametersKernel.getGroupSize()[2]);

        EXPECT_EQ(4u, capturedGroupSize.groupSizeX);
        EXPECT_EQ(2u, capturedGroupSize.groupSizeY);
        EXPECT_EQ(1u, capturedGroupSize.groupSizeZ);
        EXPECT_EQ(1u, argumentsKernel.getGroupSize()[0]);
        EXPECT_EQ(1u, argumentsKernel.getGroupSize()[1]);
        EXPECT_EQ(1u, argumentsKernel.getGroupSize()[2]);
    }
}

TEST_F(GraphTestInstantiationTest, GivenCapturedAppendKernelsWithGraphNodeDescriptorsWhenUpdatingGroupCountAndGroupSizeOfNodesThenCapturedCommandsAreUpdated) {
    GraphsCleanupGuard graphCleanup;
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    kernel.privateState.groupSize[0] = 1U;
    kernel.privateState.groupSize[1] = 1U;
    kernel.privateState.groupSize[2] = 1U;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t parametersNodeHandle = nullptr;
    ze_graph_node_exp_desc_t parametersNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    parametersNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE;
    parametersNodeDesc.phNodeHandle = &parametersNodeHandle;

    ze_graph_node_handle_t argumentsNodeHandle = nullptr;
    ze_graph_node_exp_desc_t argumentsNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    argumentsNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE;
    argumentsNodeDesc.phNodeHandle = &argumentsNodeHandle;

    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &parametersNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &argumentsNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));

    const auto &capturedCommands = srcGraph.getCapturedCommands();
    ASSERT_EQ(2u, capturedCommands.size());
    auto &parametersClosure = std::get<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>>(capturedCommands[0]);
    auto &argumentsClosure = std::get<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>>(capturedCommands[1]);
    auto argumentsKernel = argumentsClosure.indirectArgs.capturedKernel.get();
    const auto argumentsKernelGroupSize = argumentsKernel->getGroupSize();
    const std::array<uint32_t, 3> argumentsKernelGroupSizeBeforeUpdate = {argumentsKernelGroupSize[0], argumentsKernelGroupSize[1], argumentsKernelGroupSize[2]};

    ze_graph_node_update_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_SIZE_EXP_DESC};
    groupSizeDesc.groupSize = {4, 2, 1};
    ze_graph_node_update_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GROUP_COUNT_EXP_DESC};
    groupCountDesc.pNext = &groupSizeDesc;
    groupCountDesc.groupCount = {2, 3, 4};

    std::array<ze_graph_node_handle_t, 2> hNodes = {parametersNodeHandle, argumentsNodeHandle};
    std::array<const void *, 2> updateDescs = {&groupCountDesc, &groupCountDesc};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraphHandle, 2, hNodes.data(), updateDescs.data(), nullptr));

    EXPECT_EQ(2u, parametersClosure.indirectArgs.groupCounts.groupCountX);
    EXPECT_EQ(3u, parametersClosure.indirectArgs.groupCounts.groupCountY);
    EXPECT_EQ(4u, parametersClosure.indirectArgs.groupCounts.groupCountZ);
    auto parametersKernel = parametersClosure.indirectArgs.capturedKernel.get();
    EXPECT_EQ(4u, parametersKernel->getGroupSize()[0]);
    EXPECT_EQ(2u, parametersKernel->getGroupSize()[1]);
    EXPECT_EQ(1u, parametersKernel->getGroupSize()[2]);

    EXPECT_EQ(2u, argumentsClosure.apiArgs.groupCounts.groupCountX);
    EXPECT_EQ(3u, argumentsClosure.apiArgs.groupCounts.groupCountY);
    EXPECT_EQ(4u, argumentsClosure.apiArgs.groupCounts.groupCountZ);
    EXPECT_EQ(4u, argumentsClosure.apiArgs.groupSizes.groupSizeX);
    EXPECT_EQ(2u, argumentsClosure.apiArgs.groupSizes.groupSizeY);
    EXPECT_EQ(1u, argumentsClosure.apiArgs.groupSizes.groupSizeZ);
    EXPECT_EQ(argumentsKernelGroupSizeBeforeUpdate[0], argumentsKernel->getGroupSize()[0]);
    EXPECT_EQ(argumentsKernelGroupSizeBeforeUpdate[1], argumentsKernel->getGroupSize()[1]);
    EXPECT_EQ(argumentsKernelGroupSizeBeforeUpdate[2], argumentsKernel->getGroupSize()[2]);

    EXPECT_EQ(1u, kernel.getGroupSize()[0]);
    EXPECT_EQ(1u, kernel.getGroupSize()[1]);
    EXPECT_EQ(1u, kernel.getGroupSize()[2]);
}

TEST_F(GraphTestInstantiationTest, GivenCapturedAppendKernelsWithGraphNodeDescriptorsWhenUpdatingGlobalOffsetOfNodesThenCapturedKernelsAreUpdated) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    kernel.setGlobalOffsetExp(1U, 2U, 3U);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t parametersNodeHandle = nullptr;
    ze_graph_node_exp_desc_t parametersNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    parametersNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET;
    parametersNodeDesc.phNodeHandle = &parametersNodeHandle;
    ze_graph_node_handle_t argumentsNodeHandle = nullptr;
    ze_graph_node_exp_desc_t argumentsNodeDesc = parametersNodeDesc;
    argumentsNodeDesc.phNodeHandle = &argumentsNodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &parametersNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &argumentsNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));

    const auto &capturedCommands = srcGraph.getCapturedCommands();
    ASSERT_EQ(2u, capturedCommands.size());
    auto parametersKernel = std::get<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>>(capturedCommands[0]).indirectArgs.capturedKernel.get();
    auto argumentsKernel = std::get<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>>(capturedCommands[1]).indirectArgs.capturedKernel.get();

    ze_graph_node_update_global_offset_exp_desc_t parametersGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};
    parametersGlobalOffsetDesc.offsetX = 4U;
    parametersGlobalOffsetDesc.offsetY = 5U;
    parametersGlobalOffsetDesc.offsetZ = 6U;
    ze_graph_node_update_global_offset_exp_desc_t argumentsGlobalOffsetDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_UPDATE_GLOBAL_OFFSET_EXP_DESC};
    argumentsGlobalOffsetDesc.offsetX = 7U;
    argumentsGlobalOffsetDesc.offsetY = 8U;
    argumentsGlobalOffsetDesc.offsetZ = 9U;

    std::array<ze_graph_node_handle_t, 2> hNodes = {parametersNodeHandle, argumentsNodeHandle};
    std::array<const void *, 2> updateDescs = {&parametersGlobalOffsetDesc, &argumentsGlobalOffsetDesc};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphNodesUpdateExp(srcGraphHandle, 2, hNodes.data(), updateDescs.data(), nullptr));

    EXPECT_EQ(4U, parametersKernel->getGlobalOffsets()[0]);
    EXPECT_EQ(5U, parametersKernel->getGlobalOffsets()[1]);
    EXPECT_EQ(6U, parametersKernel->getGlobalOffsets()[2]);
    EXPECT_EQ(7U, argumentsKernel->getGlobalOffsets()[0]);
    EXPECT_EQ(8U, argumentsKernel->getGlobalOffsets()[1]);
    EXPECT_EQ(9U, argumentsKernel->getGlobalOffsets()[2]);

    EXPECT_EQ(1U, kernel.getGlobalOffsets()[0]);
    EXPECT_EQ(2U, kernel.getGlobalOffsets()[1]);
    EXPECT_EQ(3U, kernel.getGlobalOffsets()[2]);
}

TEST(GraphTestCommandListDestruction, GivenPreallocatedCaptureTargetWhenCommandListIsDestroyedThenGraphIsNotDestroyed) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    bool graphDestroyed = false;
    zex_mem_graph_free_callback_fn_t markGraphDestroyed = +[](void *userData) { *static_cast<bool *>(userData) = true; };
    Graph graph{&ctx, true, false};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(&graph, markGraphDestroyed, &graphDestroyed, nullptr));
    {
        MockCommandList cmdlist;
        cmdlist.setGraphCaptureTarget(&graph);
    }
    EXPECT_FALSE(graphDestroyed);
}

TEST(GraphTestCommandListDestruction, GivenNotPreallocatedCaptureTargetWhenCommandListIsDestroyedThenGraphIsDestroyed) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    bool graphDestroyed = false;
    zex_mem_graph_free_callback_fn_t markGraphDestroyed = +[](void *userData) { *static_cast<bool *>(userData) = true; };
    auto graph = std::make_unique<Graph>(&ctx, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphSetDestructionCallbackExp(graph.get(), markGraphDestroyed, &graphDestroyed, nullptr));
    {
        MockCommandList cmdlist;
        cmdlist.setGraphCaptureTarget(graph.release());
    }
    EXPECT_TRUE(graphDestroyed);
}

TEST(CaptureTestMclIdMapping, GivenGetNextCommandIdFollowedByAppendBarrierWhenGettingCommandByMclIdThenReturnsMappedFollowingCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    ze_mutable_command_id_exp_desc_t desc{};
    uint64_t commandId = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExp>(
                                     cmdlist.toHandle(), &desc, &commandId));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    auto *cmd = graph.recordedApiCommands.getCommandByMclId(commandId);
    EXPECT_NE(nullptr, cmd);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd));
}

TEST(CaptureTestMclIdMapping, GivenGetNextCommandExtIdFollowedByAppendBarrierWhenGettingCommandByMclIdThenReturnsMappedFollowingCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    ze_mutable_command_id_ext_desc_t desc{};
    uint64_t commandId = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExt>(
                                     cmdlist.toHandle(), &desc, &commandId));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    auto *cmd = graph.recordedApiCommands.getCommandByMclId(commandId);
    EXPECT_NE(nullptr, cmd);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd));
}

TEST(CaptureTestMclIdMapping, GivenMultipleMutableCommandsWithDifferentFollowingCommandsWhenMappingThenEachReferencesCorrectCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    ze_mutable_command_id_exp_desc_t desc1{};
    uint64_t mclId1 = 100U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExp>(
                                     cmdlist.toHandle(), &desc1, &mclId1));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    ze_mutable_command_id_exp_desc_t desc2{};
    uint64_t mclId2 = 200U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExp>(
                                     cmdlist.toHandle(), &desc2, &mclId2));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    auto *cmd1 = graph.recordedApiCommands.getCommandByMclId(100U);
    EXPECT_NE(nullptr, cmd1);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd1));

    auto *cmd2 = graph.recordedApiCommands.getCommandByMclId(200U);
    EXPECT_NE(nullptr, cmd2);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd2));

    EXPECT_NE(cmd1, cmd2);
}

TEST(CaptureTestMclIdMapping, GivenGetNextCommandIdWithKernelsFollowedByLaunchKernelWhenMappingThenReturnsMappedKernelCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    ze_mutable_command_id_exp_desc_t desc{};
    ze_kernel_handle_t kernels[] = {nullptr};
    uint64_t commandId = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdWithKernelsExp>(
                                     cmdlist.toHandle(), &desc, 1U, kernels, &commandId));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    auto *cmd = graph.recordedApiCommands.getCommandByMclId(commandId);
    EXPECT_NE(nullptr, cmd);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd));
}

TEST(CaptureTestMclIdMapping, GivenGetNextCommandIdWithKernelExtsFollowedByLaunchKernelWhenMappingThenReturnsMappedKernelCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    ze_mutable_command_id_ext_desc_t desc{};
    ze_kernel_handle_t kernels[] = {nullptr};
    uint64_t commandId = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdWithKernelsExt>(
                                     cmdlist.toHandle(), &desc, 1U, kernels, &commandId));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    auto *cmd = graph.recordedApiCommands.getCommandByMclId(commandId);
    EXPECT_NE(nullptr, cmd);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd));
}

TEST(CaptureTestMclIdMapping, GivenInvalidMclIdWhenGettingCommandByMclIdThenReturnsNull) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};

    uint64_t nonExistentMclId = 999U;
    auto *cmd = graph.recordedApiCommands.getCommandByMclId(nonExistentMclId);
    EXPECT_EQ(nullptr, cmd);
}

TEST(CaptureTestMclIdMapping, GivenMutableCommandsBetweenRegularCommandsWhenMappingThenOnlyNextCommandsAreMapped) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                         cmdlist.toHandle(), nullptr, 0U, nullptr));
    }

    ze_mutable_command_id_exp_desc_t desc{};
    uint64_t mclId = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExp>(
                                     cmdlist.toHandle(), &desc, &mclId));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    for (int i = 0; i < 1; ++i) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                         cmdlist.toHandle(), nullptr, 0U, nullptr));
    }

    auto *cmd = graph.recordedApiCommands.getCommandByMclId(mclId);
    EXPECT_NE(nullptr, cmd);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd));

    EXPECT_EQ(5U, graph.recordedApiCommands.size());
}

TEST(CaptureTestMclIdMapping, GivenGetNextCommandIdFollowedBySpecificCommandWhenMappingThenMclIdReferencesFollowingCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    ze_mutable_command_id_exp_desc_t desc{};
    desc.flags = 42U;
    uint64_t mclId = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExp>(
                                     cmdlist.toHandle(), &desc, &mclId));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    auto *cmd = graph.recordedApiCommands.getCommandByMclId(mclId);
    EXPECT_NE(nullptr, cmd);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd));
}

TEST(CaptureTestMclIdMapping, GivenSequenceOfMutableCommandsWithFollowingCommandsWhenMappingThenEachReferencesNextCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    std::vector<uint64_t> mclIds;
    for (int i = 0; i < 3; ++i) {
        ze_mutable_command_id_exp_desc_t desc{};
        uint64_t mclId = 1000U + (i * 100U);
        EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExp>(
                                         cmdlist.toHandle(), &desc, &mclId));
        mclIds.push_back(mclId);

        EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                         cmdlist.toHandle(), nullptr, 0U, nullptr));
    }

    std::set<uint64_t> uniqueIds(mclIds.begin(), mclIds.end());
    EXPECT_EQ(3U, uniqueIds.size());

    for (auto mclId : mclIds) {
        auto *cmd = graph.recordedApiCommands.getCommandByMclId(mclId);
        EXPECT_NE(nullptr, cmd);
        EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd));
    }

    EXPECT_EQ(6U, graph.recordedApiCommands.size());
}

TEST(CaptureTestMclIdMapping, GivenBothMutableVariantsWithFollowingCommandsWhenMappingThenBothReferenceCorrectNextCommands) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = nullptr;

    MockGraph graph{&ctx, true, false};

    ze_mutable_command_id_exp_desc_t desc1{};
    uint64_t mclId1 = 500U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdExp>(
                                     cmdlist.toHandle(), &desc1, &mclId1));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    ze_mutable_command_id_exp_desc_t desc2{};
    ze_kernel_handle_t kernels[] = {nullptr};
    uint64_t mclId2 = 600U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListGetNextCommandIdWithKernelsExp>(
                                     cmdlist.toHandle(), &desc2, 1U, kernels, &mclId2));

    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.recordedApiCommands.capture<L0::CaptureApi::zeCommandListAppendBarrier>(
                                     cmdlist.toHandle(), nullptr, 0U, nullptr));

    auto *cmd1 = graph.recordedApiCommands.getCommandByMclId(500U);
    EXPECT_NE(nullptr, cmd1);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd1));

    auto *cmd2 = graph.recordedApiCommands.getCommandByMclId(600U);
    EXPECT_NE(nullptr, cmd2);
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(*cmd2));

    EXPECT_NE(cmd1, cmd2);

    EXPECT_EQ(4U, graph.recordedApiCommands.size());
}

TEST_F(GraphTestApiPauseResume, GivenCapturePausedAndResumedWhenAppendingCommandsThenOnlyCommandsOutsidePauseAreCaptured) {
    GraphsCleanupGuard graphCleanup;

    ze_graph_handle_t hGraph{};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphCreateExp(context->toHandle(), &hGraph, nullptr));
    ASSERT_NE(nullptr, hGraph);

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, hGraph, nullptr));

    auto *graph = L0::Graph::fromHandle(hGraph);
    ASSERT_NE(nullptr, graph);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0U, nullptr));
    ASSERT_EQ(1u, graph->getCapturedCommands().size());
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(graph->getCapturedCommands()[0].index()));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphPauseCaptureExt(hGraph));

    // paused capture, command list should not have graph associated
    ze_graph_handle_t queriedGraph{};
    EXPECT_EQ(ZE_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING, L0::zeCommandListGetGraphExp(immCmdListHandle, &queriedGraph));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0U, nullptr));
    // state of the graph should be unchanged
    EXPECT_EQ(1u, graph->getCapturedCommands().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphResumeCaptureExt(hGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListGetGraphExp(immCmdListHandle, &queriedGraph));
    EXPECT_EQ(hGraph, queriedGraph);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0U, nullptr));
    ASSERT_EQ(2u, graph->getCapturedCommands().size());
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(graph->getCapturedCommands()[1].index()));

    ze_graph_handle_t hEndedGraph{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &hEndedGraph, nullptr));
    EXPECT_EQ(hGraph, hEndedGraph);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExp(hGraph));
}

TEST_F(GraphTestApiPauseResume, GivenGraphNotCapturingWhenCallingGraphPauseResumeThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    auto hGraph = graph.toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING, L0::zeGraphPauseCaptureExt(hGraph));
    EXPECT_EQ(ZE_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING, L0::zeGraphResumeCaptureExt(hGraph));
}

TEST_F(GraphTestApiPauseResume, GivenActiveCaptureWhenPausingAndResumingThenCommandListCaptureTargetIsDetachedAndRestored) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockCommandList cmdList;
    TestableGraphForPrimaryCommandListQuery graph{&ctx, true, false};
    auto hGraph = graph.toHandle();

    graph.setCaptureSource(&cmdList);
    cmdList.setGraphCaptureTarget(&graph);

    EXPECT_EQ(&graph, cmdList.getGraphCaptureTarget());
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphPauseCaptureExt(hGraph));
    EXPECT_EQ(nullptr, cmdList.getGraphCaptureTarget());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphPauseCaptureExt(hGraph));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphResumeCaptureExt(hGraph));
    EXPECT_EQ(&graph, cmdList.getGraphCaptureTarget());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeGraphResumeCaptureExt(hGraph));

    cmdList.setGraphCaptureTarget(nullptr);
}

TEST_F(GraphTestApiPauseResume, GivenRootGraphWithSubgraphWhenPausingRootThenAllCapturesArePaused) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockCommandList rootCmdlist;
    rootCmdlist.device = this->device;
    MockCommandList childCmdlist;
    childCmdlist.device = this->device;

    Graph rootGraph{&ctx, true, false};
    rootGraph.startCapturingFrom(rootCmdlist, false);

    Mock<Event> forkEvent;
    rootGraph.capture<CaptureApi::zeCommandListAppendBarrier>(rootCmdlist.toHandle(), forkEvent.toHandle(), 0U, nullptr);
    rootGraph.registerSignallingEventFromPreviousCommand(forkEvent);

    Graph *subGraph = nullptr;
    rootGraph.forkTo(childCmdlist, subGraph, forkEvent);
    ASSERT_NE(nullptr, subGraph);

    Mock<Event> subGraphSignalEvent;
    subGraph->capture<CaptureApi::zeCommandListAppendBarrier>(childCmdlist.toHandle(), subGraphSignalEvent.toHandle(), 0U, nullptr);
    subGraph->registerSignallingEventFromPreviousCommand(subGraphSignalEvent);

    EXPECT_EQ(&rootGraph, rootCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(subGraph, childCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(&rootCmdlist, forkEvent.getRecordedSignalFrom());
    EXPECT_EQ(&childCmdlist, subGraphSignalEvent.getRecordedSignalFrom());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphPauseCaptureExt(rootGraph.toHandle()));
    EXPECT_EQ(nullptr, rootCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(nullptr, childCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(nullptr, forkEvent.getRecordedSignalFrom());
    EXPECT_EQ(nullptr, subGraphSignalEvent.getRecordedSignalFrom());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphResumeCaptureExt(rootGraph.toHandle()));
    EXPECT_EQ(&rootGraph, rootCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(subGraph, childCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(&rootCmdlist, forkEvent.getRecordedSignalFrom());
    EXPECT_EQ(&childCmdlist, subGraphSignalEvent.getRecordedSignalFrom());

    rootCmdlist.setGraphCaptureTarget(nullptr);
    childCmdlist.setGraphCaptureTarget(nullptr);
}

TEST_F(GraphTestApiPauseResume, GivenRootGraphWithSubgraphWhenPausingSubgraphOnlyThenOnlySubgraphSignalsArePaused) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockCommandList rootCmdlist;
    rootCmdlist.device = this->device;
    MockCommandList childCmdlist;
    childCmdlist.device = this->device;

    Graph rootGraph{&ctx, true, false};
    rootGraph.startCapturingFrom(rootCmdlist, false);

    Mock<Event> forkEvent;
    rootGraph.capture<CaptureApi::zeCommandListAppendBarrier>(rootCmdlist.toHandle(), forkEvent.toHandle(), 0U, nullptr);
    rootGraph.registerSignallingEventFromPreviousCommand(forkEvent);

    Graph *subGraph = nullptr;
    rootGraph.forkTo(childCmdlist, subGraph, forkEvent);
    ASSERT_NE(nullptr, subGraph);

    Mock<Event> subGraphSignalEvent;
    subGraph->capture<CaptureApi::zeCommandListAppendBarrier>(childCmdlist.toHandle(), subGraphSignalEvent.toHandle(), 0U, nullptr);
    subGraph->registerSignallingEventFromPreviousCommand(subGraphSignalEvent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphPauseCaptureExt(subGraph->toHandle()));
    EXPECT_EQ(&rootGraph, rootCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(nullptr, childCmdlist.getGraphCaptureTarget());
    EXPECT_EQ(&rootCmdlist, forkEvent.getRecordedSignalFrom());
    EXPECT_EQ(nullptr, subGraphSignalEvent.getRecordedSignalFrom());

    rootGraph.capture<CaptureApi::zeCommandListAppendBarrier>(rootCmdlist.toHandle(), subGraphSignalEvent.toHandle(), 0U, nullptr);
    rootGraph.registerSignallingEventFromPreviousCommand(subGraphSignalEvent);
    EXPECT_EQ(&rootCmdlist, subGraphSignalEvent.getRecordedSignalFrom());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphResumeCaptureExt(subGraph->toHandle()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphPauseCaptureExt(subGraph->toHandle()));
    EXPECT_EQ(&rootCmdlist, subGraphSignalEvent.getRecordedSignalFrom());

    rootCmdlist.setGraphCaptureTarget(nullptr);
    childCmdlist.setGraphCaptureTarget(nullptr);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenStoppedGraphWithSubgraphWhenSignallingEventsAreReassignedBeforeGraphDestructionThenGraphDestructionDoesNotModifyEvents) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockCommandList rootCmdlist;
    rootCmdlist.device = this->device;
    MockCommandList childCmdlist;
    childCmdlist.device = this->device;
    MockCommandList unrelatedCmdlist;

    auto rootGraph = std::make_unique<Graph>(&ctx, true, false);
    rootGraph->startCapturingFrom(rootCmdlist, false);
    rootCmdlist.setGraphCaptureTarget(rootGraph.get());

    Mock<Event> forkEvent;
    rootGraph->capture<CaptureApi::zeCommandListAppendBarrier>(rootCmdlist.toHandle(), forkEvent.toHandle(), 0U, nullptr);
    rootGraph->registerSignallingEventFromPreviousCommand(forkEvent);

    Graph *subGraph = nullptr;
    rootGraph->forkTo(childCmdlist, subGraph, forkEvent);
    ASSERT_NE(nullptr, subGraph);

    Mock<Event> subGraphSignalEvent;
    subGraph->capture<CaptureApi::zeCommandListAppendBarrier>(childCmdlist.toHandle(), subGraphSignalEvent.toHandle(), 0U, nullptr);
    subGraph->registerSignallingEventFromPreviousCommand(subGraphSignalEvent);

    rootGraph->stopCapturing();
    EXPECT_EQ(nullptr, forkEvent.getRecordedSignalFrom());
    EXPECT_EQ(nullptr, subGraphSignalEvent.getRecordedSignalFrom());

    auto unrelatedEntry = std::make_shared<GraphInternalEventEntry>(GraphInternalEventEntry{.signalFrom = &unrelatedCmdlist});
    forkEvent.setRecordedSignalEntry(unrelatedEntry);
    subGraphSignalEvent.setRecordedSignalEntry(unrelatedEntry);

    rootGraph.reset();
    EXPECT_EQ(&unrelatedCmdlist, forkEvent.getRecordedSignalFrom());
    EXPECT_EQ(&unrelatedCmdlist, subGraphSignalEvent.getRecordedSignalFrom());

    childCmdlist.setGraphCaptureTarget(nullptr);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenGraphStillCapturingWhenGraphIsDestroyedThenSignallingEventsAreUnregistered) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockCommandList cmdlist;
    cmdlist.device = this->device;

    auto graph = std::make_unique<Graph>(&ctx, true, false);
    graph->startCapturingFrom(cmdlist, false);

    Mock<Event> signalEvent;
    graph->capture<CaptureApi::zeCommandListAppendBarrier>(cmdlist.toHandle(), signalEvent.toHandle(), 0U, nullptr);
    graph->registerSignallingEventFromPreviousCommand(signalEvent);
    EXPECT_EQ(&cmdlist, signalEvent.getRecordedSignalFrom());

    cmdlist.setGraphCaptureTarget(nullptr);
    graph.reset();
    EXPECT_EQ(nullptr, signalEvent.getRecordedSignalFrom());
}

} // namespace ult
} // namespace L0
