/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
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

struct GraphExternalWaitEventFixtureInit : public GraphFixture {
    void setUp(int32_t contextGroupSize) {
        debugManager.flags.ContextGroupSize.set(contextGroupSize);
        GraphFixture::setUp();
    }

    template <typename FamilyType>
    void testExternalWaitEventRootChild(bool externalWaitRoot, bool externalWaitChild);
};

template <int32_t contextGroupSize>
struct GraphExternalWaitEventFixture : public GraphExternalWaitEventFixtureInit {
    void setUp() {
        GraphExternalWaitEventFixtureInit::setUp(contextGroupSize);
    }
};

using GraphTestApiSubmit = Test<GraphFixture>;
using GraphTestApiInstantiate = Test<GraphFixture>;
using GraphInstantiation = Test<GraphFixture>;
using GraphTestDebugApis = Test<GraphFixture>;
using GraphInstantiationValidation = Test<GraphFixture>;

using GraphTestInstantiationExternalWaitEventNoMultiEngineTest = Test<GraphExternalWaitEventFixture<0>>;

struct GraphImplicitSyncMutationFixture : public GraphFixture {
    void setUp() {
        GraphFixture::setUp();
        debugManager.flags.ForceGraphImplicitSyncMutation.set(1);
    }
};
using GraphImplicitSyncMutationTest = Test<GraphImplicitSyncMutationFixture>;

struct GraphImplicitSyncMutationExternalWaitEventFixture : public GraphImplicitSyncMutationFixture {
    void setUp() {
        GraphImplicitSyncMutationFixture::setUp();

        ze_result_t returnValue = ZE_RESULT_ERROR_UNKNOWN;
        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
        this->inOrderCmdList.reset(CommandList::createImmediate(this->device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        this->inOrderCmdList->setOrdinal(0);
        auto hCmdList = this->inOrderCmdList->toHandle();

        ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
        eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(this->context->toHandle(), this->device->toHandle(), &eventDesc, &this->externalEvent));
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(hCmdList, this->externalEvent, 0U, nullptr));

        this->srcGraph = std::make_unique<L0::Graph>(this->context, true, false);
        auto hGraph = this->srcGraph->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(hCmdList, nullptr, 1U, &this->externalEvent));
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));
    }

    void tearDown() {
        this->srcGraph.reset();
        if (nullptr != this->externalEvent) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(this->externalEvent));
        }
        this->inOrderCmdList.reset();
        GraphImplicitSyncMutationFixture::tearDown();
    }

    GraphsCleanupGuard graphCleanup;
    std::unique_ptr<L0::CommandList> inOrderCmdList;
    std::unique_ptr<L0::Graph> srcGraph;
    ze_event_handle_t externalEvent = nullptr;
};
using GraphImplicitSyncMutationExternalWaitEventTest = Test<GraphImplicitSyncMutationExternalWaitEventFixture>;
using GraphTestInstantiationExternalWaitEventMultiEngineTest = Test<GraphExternalWaitEventFixture<64>>;

struct LaunchKernelWithArgumentsVisitorCapture {
    ze_command_list_handle_t hCommandList = nullptr;
    ze_kernel_handle_t hKernel = nullptr;
    ze_group_count_t groupCounts = {0, 0, 0};
    ze_group_size_t groupSizes = {0, 0, 0};
    bool wasCalled = false;
};

static ze_result_t ZE_VISITOR_CCONV launchKernelWithArgumentsVisitor(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    ze_group_count_t groupCounts,
    ze_group_size_t groupSizes,
    void **,
    const void *,
    ze_event_handle_t,
    uint32_t,
    ze_event_handle_t *,
    void *userData) {

    auto *capture = reinterpret_cast<LaunchKernelWithArgumentsVisitorCapture *>(userData);
    capture->hCommandList = hCommandList;
    capture->hKernel = hKernel;
    capture->groupCounts = groupCounts;
    capture->groupSizes = groupSizes;
    capture->wasCalled = true;
    return ZE_RESULT_SUCCESS;
}

TEST(GraphTestApiCreate, GivenNonNullPNextThenGraphCreateReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    auto ctxHandle = ctx.toHandle();
    ze_graph_handle_t graphHandle = nullptr;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;
    auto err = L0::zeGraphCreateExp(ctxHandle, &graphHandle, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graphHandle);
}

TEST(GraphTestApiCreate, GivenNullContextThenGraphCreateReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ze_graph_handle_t graphHandle = nullptr;
    auto err = L0::zeGraphCreateExp(nullptr, &graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graphHandle);
}

TEST(GraphTestApiCreate, GivenValidContextThenGraphCreateReturnsSuccess) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    auto ctxHandle = ctx.toHandle();
    ze_graph_handle_t graphHandle = nullptr;
    auto err = L0::zeGraphCreateExp(ctxHandle, &graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, graphHandle);

    err = L0::zeGraphDestroyExp(graphHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCreate, GivenInvalidGraphThenGraphDestroyReturnsError) {
    GraphsCleanupGuard graphCleanup;
    auto err = zeGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenGraphsEnabledWhenForkingToRegularOrSynchronousCmdlistThenFail) {
    GraphsCleanupGuard graphCleanup;

    Mock<CommandList> parentCmdList;
    Mock<Event> event;
    auto signalEntry = std::make_shared<GraphInternalEventEntry>(GraphInternalEventEntry{.signalFrom = &parentCmdList});
    event.setRecordedSignalEntry(signalEntry);
    auto hEvent = event.toHandle();

    Mock<CommandList> mockCmdList;
    auto hCmdList = mockCmdList.toHandle();
    auto &cmdList = static_cast<Mock<CommandList>::BaseClass &>(mockCmdList);

    processUsesGraphs.store(true);
    EXPECT_TRUE(areGraphsEnabled());
    EXPECT_FALSE(cmdList.isImmediateType());
    auto ret1 = cmdList.capture<CaptureApi::zeCommandListAppendBarrier>(hCmdList, nullptr, 1U, &hEvent);
    EXPECT_EQ(ret1, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);
    EXPECT_TRUE(event.getRecordedSignalFromCalled);

    mockCmdList.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    mockCmdList.isSyncModeQueue = true;
    auto ret2 = cmdList.capture<CaptureApi::zeCommandListAppendBarrier>(hCmdList, nullptr, 1U, &hEvent);
    EXPECT_EQ(ret2, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenGraphsEnabledWhenRegularCmdlistWaitsOnNotRecordedEventsThenForkIsDetectedOnlyIfAnyEventIsRecorded) {
    GraphsCleanupGuard graphCleanup;

    Mock<CommandList> parentCmdList;
    Mock<Event> recordedEvent;
    auto signalEntry = std::make_shared<GraphInternalEventEntry>(GraphInternalEventEntry{.signalFrom = &parentCmdList});
    recordedEvent.setRecordedSignalEntry(signalEntry);
    Mock<Event> notRecordedEvent;

    Mock<CommandList> mockCmdList;
    auto hCmdList = mockCmdList.toHandle();
    auto &cmdList = static_cast<Mock<CommandList>::BaseClass &>(mockCmdList);

    processUsesGraphs.store(true);
    EXPECT_FALSE(cmdList.isImmediateType());

    ze_event_handle_t notRecordedOnly[] = {notRecordedEvent.toHandle()};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, cmdList.capture<CaptureApi::zeCommandListAppendBarrier>(hCmdList, nullptr, 1U, notRecordedOnly));
    EXPECT_NE(0u, notRecordedEvent.getRecordedSignalFromCalled);

    ze_event_handle_t notRecordedFirst[] = {notRecordedEvent.toHandle(), recordedEvent.toHandle()};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE, cmdList.capture<CaptureApi::zeCommandListAppendBarrier>(hCmdList, nullptr, 2U, notRecordedFirst));
    EXPECT_NE(0u, recordedEvent.getRecordedSignalFromCalled);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenRegularCommandListWithFlatCaptureWhenAppendingBarrierThenApiCallIsRecorded) {
    GraphsCleanupGuard graphCleanup;

    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeRegular;
    cmdlist.flatCapture = std::make_unique<RecordedApiCommands>();

    auto result = zeCommandListAppendBarrier(cmdlist.toHandle(), nullptr, 0U, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, cmdlist.flatCapture);
    ASSERT_EQ(1u, cmdlist.flatCapture->size());
    EXPECT_TRUE(std::holds_alternative<Closure<CaptureApi::zeCommandListAppendBarrier>>(cmdlist.flatCapture->commands[0]));
}

TEST_F(GraphExecution, GivenExecutableGraphSubmissionNodeWhenAllocatedThenFlatCaptureIsDisabledOnReturnedCommandList) {
    GraphsCleanupGuard graphCleanup;

    struct ExposedExecutableGraph : ExecutableGraph {
        using ExecutableGraph::allocateAndAddCommandListSubmissionNode;
        using ExecutableGraph::src;
    };

    struct ExposedGraph : Graph {
        using Graph::captureTargetDesc;
        using Graph::Graph;
    };

    MockGraphContextReturningSpecificCmdList ctx;
    auto *submissionCmdList = new Mock<CommandList>;
    submissionCmdList->flatCapture = std::make_unique<RecordedApiCommands>();
    ctx.cmdListsToReturn.push_back(submissionCmdList);

    ExposedGraph srcGraph(&ctx, true, false);
    srcGraph.captureTargetDesc.hDevice = device->toHandle();

    ExposedExecutableGraph execGraph;
    execGraph.src = &srcGraph;

    auto *allocatedCmdList = execGraph.allocateAndAddCommandListSubmissionNode(0);

    EXPECT_EQ(submissionCmdList, allocatedCmdList);
    EXPECT_EQ(nullptr, submissionCmdList->flatCapture);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = L0::zeCommandListBeginGraphCaptureExp(cmdListHandle, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);

    L0::Graph graph(&ctx, true, false);
    auto graphHandle = graph.toHandle();
    err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenMutableGraphDescAndMutableGraphSupportedWhenGraphCreateIsCalledThenGraphAndItsCommandListsAreMutable) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideRecordReplayGraphCapability.set(static_cast<int32_t>(ZE_RECORD_REPLAY_GRAPH_EXT_FLAG_MUTABLE_GRAPH));

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    ze_graph_handle_t graphHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphCreateExp(context->toHandle(), &graphHandle, &mutableGraphDesc));
    ASSERT_NE(nullptr, graphHandle);

    auto graph = L0::Graph::fromHandle(graphHandle);
    EXPECT_TRUE(graph->isMutableGraph());
    EXPECT_TRUE(graph->isMutableCommandList());
    ASSERT_NE(nullptr, graph->getCaptureTargetDesc().desc.pNext);
    EXPECT_EQ(ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXT_DESC, reinterpret_cast<const ze_base_desc_t *>(graph->getCaptureTargetDesc().desc.pNext)->stype);

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, graphHandle, nullptr));
    EXPECT_TRUE(graph->isMutableGraph());
    EXPECT_TRUE(graph->isMutableCommandList());
    ASSERT_NE(nullptr, graph->getCaptureTargetDesc().desc.pNext);
    EXPECT_EQ(ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXT_DESC, reinterpret_cast<const ze_base_desc_t *>(graph->getCaptureTargetDesc().desc.pNext)->stype);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExp(graphHandle));
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenMutableGraphDescAndMutableGraphNotSupportedWhenGraphCreateIsCalledThenUnsupportedFeatureIsReturned) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideRecordReplayGraphCapability.set(static_cast<int32_t>(ZE_RECORD_REPLAY_GRAPH_EXT_FLAG_IMMUTABLE_GRAPH));

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    ze_graph_handle_t graphHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeGraphCreateExp(context->toHandle(), &graphHandle, &mutableGraphDesc));
    EXPECT_EQ(nullptr, graphHandle);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenMutableGraphDescAndMutableGraphSupportedWhenBeginGraphCaptureIsCalledThenGraphSubgraphAndTheirCommandListsAreMutable) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideRecordReplayGraphCapability.set(static_cast<int32_t>(ZE_RECORD_REPLAY_GRAPH_EXT_FLAG_MUTABLE_GRAPH));

    auto forkEventHandle = createCounterBasedEvent(context, device, false);
    auto joinEventHandle = createCounterBasedEvent(context, device, false);

    ze_command_list_handle_t childCmdListHandle = nullptr;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &childCmdListHandle));

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginGraphCaptureExp(immCmdListHandle, &mutableGraphDesc));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, forkEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(childCmdListHandle, joinEventHandle, 1U, &forkEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 1U, &joinEventHandle));

    ze_graph_handle_t graphHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &graphHandle, nullptr));
    ASSERT_NE(nullptr, graphHandle);

    auto graph = L0::Graph::fromHandle(graphHandle);
    EXPECT_TRUE(graph->isMutableGraph());
    EXPECT_TRUE(graph->isMutableCommandList());
    ASSERT_NE(nullptr, graph->getCaptureTargetDesc().desc.pNext);
    EXPECT_EQ(ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXT_DESC, reinterpret_cast<const ze_base_desc_t *>(graph->getCaptureTargetDesc().desc.pNext)->stype);

    ASSERT_EQ(1U, graph->getSubgraphs().size());
    auto subgraph = graph->getSubgraphs()[0];
    EXPECT_TRUE(subgraph->isMutableGraph());
    EXPECT_TRUE(subgraph->isMutableCommandList());
    ASSERT_NE(nullptr, subgraph->getCaptureTargetDesc().desc.pNext);
    EXPECT_EQ(ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXT_DESC, reinterpret_cast<const ze_base_desc_t *>(subgraph->getCaptureTargetDesc().desc.pNext)->stype);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphDestroyExp(graphHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(childCmdListHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(forkEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(joinEventHandle));
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenMutableGraphDescAndMutableGraphNotSupportedWhenBeginGraphCaptureIsCalledThenUnsupportedFeatureIsReturned) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideRecordReplayGraphCapability.set(static_cast<int32_t>(ZE_RECORD_REPLAY_GRAPH_EXT_FLAG_IMMUTABLE_GRAPH));

    ze_mutable_graph_exp_desc_t mutableGraphDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_EXP_DESC};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListBeginGraphCaptureExp(immCmdListHandle, &mutableGraphDesc));
    EXPECT_FALSE(immCmdList->isCapturingGraph());
    EXPECT_EQ(nullptr, immCmdList->getGraphCaptureTarget());
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenNullDestinyGraphThenBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenValidDestinyGraphThenBeginCaptureReturnsSuccessAndOutputGraphIsTheSameAsInput) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;
    cmdlist.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    auto cmdListHandle = cmdlist.toHandle();
    L0::Graph graph(&ctx, true, false);
    auto graphHandle = graph.toHandle();
    auto err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, graphHandle);

    EXPECT_EQ(static_cast<ze_command_list_flags_t>(ZE_COMMAND_LIST_FLAG_IN_ORDER), graph.getCaptureTargetDesc().desc.flags);
}

TEST_F(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphEndCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = L0::zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST_F(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureReturnsNewGraph) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = L0::zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, retGraph);

    L0::zeGraphDestroyExp(retGraph);
}

TEST_F(GraphTestApiCaptureBeginEnd, WhenCommandListIsNotRecordingThenEndCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    ze_graph_handle_t retGraph = nullptr;
    auto err = L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST_F(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureRequiresOutputGraphPlaceholder) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = L0::zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListEndGraphCaptureExp(cmdListHandle, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiCaptureBeginEnd, WhenDestinyGraphProvidedThenEndCaptureDoesNotRequireOutputGraphPlaceholder) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    L0::Graph graph(&ctx, true, false);
    auto graphHandle = graph.toHandle();
    auto err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListEndGraphCaptureExp(cmdListHandle, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiCaptureBeginEnd, WhenCommandListIsAlreadyRecordingThenBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;

    L0::Graph graph1(&ctx, true, false);
    auto graphHandle1 = graph1.toHandle();
    L0::Graph graph2(&ctx, true, false);
    auto graphHandle2 = graph2.toHandle();
    auto cmdlistHandle = cmdlist.toHandle();
    auto err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdlistHandle, graphHandle1, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListBeginCaptureIntoGraphExp(cmdlistHandle, graphHandle2, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListBeginGraphCaptureExp(cmdlistHandle, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = L0::zeCommandListEndGraphCaptureExp(cmdlistHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, graphHandle1);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenZeCommandListAppendLaunchKernelWithArgumentsWhenIndirectArgsCreatedThenProperKernelStateRecorded) {
    GraphsCleanupGuard graphCleanup;

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    L0::ClosureExternalStorage storage;

    constexpr auto apiValue = CaptureApi::zeCommandListAppendLaunchKernelWithArguments;
    constexpr uint32_t expectedGroupSizes[] = {5U, 6U, 7U};

    auto apiArgs = Closure<apiValue>::ApiArgs{
        .hCommandList = immCmdListHandle,
        .kernelHandle = kernel.toHandle(),
        .groupCounts = {0, 0, 0},
        .groupSizes = {
            .groupSizeX = expectedGroupSizes[0],
            .groupSizeY = expectedGroupSizes[1],
            .groupSizeZ = expectedGroupSizes[2],
        },
        .pArguments = nullptr,
        .pNext = nullptr,
        .hSignalEvent = nullptr,
        .numWaitEvents = 0,
        .phWaitEvents = nullptr};

    kernel.privateState.groupSize[0] = 1U;
    kernel.privateState.groupSize[1] = 1U;
    kernel.privateState.groupSize[2] = 1U;

    auto indirectArgs = Closure<apiValue>::IndirectArgs(apiArgs, storage);

    EXPECT_EQ(kernel.getGroupSize()[0], expectedGroupSizes[0]);
    EXPECT_EQ(kernel.getGroupSize()[1], expectedGroupSizes[1]);
    EXPECT_EQ(kernel.getGroupSize()[2], expectedGroupSizes[2]);
    EXPECT_EQ(indirectArgs.capturedKernel->getGroupSize()[0], expectedGroupSizes[0]);
    EXPECT_EQ(indirectArgs.capturedKernel->getGroupSize()[1], expectedGroupSizes[1]);
    EXPECT_EQ(indirectArgs.capturedKernel->getGroupSize()[2], expectedGroupSizes[2]);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenZeCommandListAppendLaunchKernelWithArgumentsWhenInvokingVisitorThenGroupCountAndSizeArePassedByValue) {
    GraphsCleanupGuard graphCleanup;

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    L0::ClosureExternalStorage storage;

    constexpr auto apiValue = CaptureApi::zeCommandListAppendLaunchKernelWithArguments;
    auto apiArgs = Closure<apiValue>::ApiArgs{
        .hCommandList = immCmdListHandle,
        .kernelHandle = kernel.toHandle(),
        .groupCounts = {2, 3, 4},
        .groupSizes = {5, 6, 7},
        .pArguments = nullptr,
        .pNext = nullptr,
        .hSignalEvent = nullptr,
        .numWaitEvents = 0,
        .phWaitEvents = nullptr};

    Closure<apiValue> closure(apiArgs, storage);

    LaunchKernelWithArgumentsVisitorCapture capture{};
    auto result = closure.invokeVisitor(reinterpret_cast<void *>(launchKernelWithArgumentsVisitor), &capture, storage);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(capture.wasCalled);
    EXPECT_EQ(apiArgs.hCommandList, capture.hCommandList);
    EXPECT_EQ(closure.indirectArgs.capturedKernel.get(), capture.hKernel);
    EXPECT_EQ(apiArgs.groupCounts.groupCountX, capture.groupCounts.groupCountX);
    EXPECT_EQ(apiArgs.groupCounts.groupCountY, capture.groupCounts.groupCountY);
    EXPECT_EQ(apiArgs.groupCounts.groupCountZ, capture.groupCounts.groupCountZ);
    EXPECT_EQ(apiArgs.groupSizes.groupSizeX, capture.groupSizes.groupSizeX);
    EXPECT_EQ(apiArgs.groupSizes.groupSizeY, capture.groupSizes.groupSizeY);
    EXPECT_EQ(apiArgs.groupSizes.groupSizeZ, capture.groupSizes.groupSizeZ);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenAppendKernelWithParametersClosureWithGraphNodeWhenMovedThenGraphNodeHandleAddressIsStableAndGraphNodeReferencesFollowMoveTarget) {
    GraphsCleanupGuard graphCleanup;

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    L0::ClosureExternalStorage storage;
    storage.graphNodeCreationEnabled = true;

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};

    constexpr auto apiValue = CaptureApi::zeCommandListAppendLaunchKernelWithParameters;
    auto apiArgs = Closure<apiValue>::ApiArgs{
        .hCommandList = immCmdListHandle,
        .kernelHandle = kernel.toHandle(),
        .pGroupCounts = &groupCount,
        .pNext = &graphNodeDesc,
        .hSignalEvent = nullptr,
        .numWaitEvents = 0,
        .phWaitEvents = nullptr};
    auto apiArgsWithoutGraphNode = apiArgs;
    apiArgsWithoutGraphNode.pNext = nullptr;

    Closure<apiValue> closure(apiArgs, storage);
    ASSERT_EQ(ZE_RESULT_SUCCESS, storage.lastResult);
    ASSERT_NE(nullptr, nodeHandle);
    const auto *capturedLaunchKernelState = closure.indirectArgs.capturedLaunchKernelState.get();
    EXPECT_EQ(nodeHandle, capturedLaunchKernelState->graphNodeHandle);
    auto clonedGraphNodeDesc = static_cast<ze_graph_node_exp_desc_t *>(capturedLaunchKernelState->clonedPNext);
    EXPECT_EQ(&capturedLaunchKernelState->graphNodeHandle, clonedGraphNodeDesc->phNodeHandle);
    auto graphNode = L0::GraphNode::fromHandle(nodeHandle);
    EXPECT_EQ(&closure.indirectArgs.groupCounts, graphNode->getGroupCounts());
    EXPECT_EQ(nullptr, graphNode->getGroupSizes());
    EXPECT_EQ(closure.indirectArgs.capturedKernel.get(), graphNode->getKernel());

    auto movedClosure = std::move(closure);
    EXPECT_EQ(capturedLaunchKernelState, movedClosure.indirectArgs.capturedLaunchKernelState.get());
    EXPECT_EQ(&capturedLaunchKernelState->graphNodeHandle, clonedGraphNodeDesc->phNodeHandle);
    EXPECT_EQ(&movedClosure.indirectArgs.groupCounts, graphNode->getGroupCounts());
    EXPECT_EQ(nullptr, graphNode->getGroupSizes());
    EXPECT_EQ(movedClosure.indirectArgs.capturedKernel.get(), graphNode->getKernel());

    Closure<apiValue> assignedClosure(apiArgsWithoutGraphNode, storage);
    assignedClosure = std::move(movedClosure);
    EXPECT_EQ(capturedLaunchKernelState, assignedClosure.indirectArgs.capturedLaunchKernelState.get());
    EXPECT_EQ(&capturedLaunchKernelState->graphNodeHandle, clonedGraphNodeDesc->phNodeHandle);
    EXPECT_EQ(&assignedClosure.indirectArgs.groupCounts, graphNode->getGroupCounts());
    EXPECT_EQ(nullptr, graphNode->getGroupSizes());
    EXPECT_EQ(assignedClosure.indirectArgs.capturedKernel.get(), graphNode->getKernel());
}

TEST_F(GraphTestApiCaptureWithDevice, GivenAppendKernelWithArgumentsClosureWithGraphNodeWhenMovedThenGraphNodeHandleAddressIsStableAndGraphNodeReferencesFollowMoveTarget) {
    GraphsCleanupGuard graphCleanup;

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    L0::ClosureExternalStorage storage;
    storage.graphNodeCreationEnabled = true;

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;

    constexpr auto apiValue = CaptureApi::zeCommandListAppendLaunchKernelWithArguments;
    auto apiArgs = Closure<apiValue>::ApiArgs{
        .hCommandList = immCmdListHandle,
        .kernelHandle = kernel.toHandle(),
        .groupCounts = {1, 1, 1},
        .groupSizes = {1, 1, 1},
        .pArguments = nullptr,
        .pNext = &graphNodeDesc,
        .hSignalEvent = nullptr,
        .numWaitEvents = 0,
        .phWaitEvents = nullptr};
    auto apiArgsWithoutGraphNode = apiArgs;
    apiArgsWithoutGraphNode.pNext = nullptr;

    Closure<apiValue> closure(apiArgs, storage);
    ASSERT_EQ(ZE_RESULT_SUCCESS, storage.lastResult);
    ASSERT_NE(nullptr, nodeHandle);
    const auto *capturedLaunchKernelState = closure.indirectArgs.capturedLaunchKernelState.get();
    EXPECT_EQ(nodeHandle, capturedLaunchKernelState->graphNodeHandle);
    auto clonedGraphNodeDesc = static_cast<ze_graph_node_exp_desc_t *>(capturedLaunchKernelState->clonedPNext);
    EXPECT_EQ(&capturedLaunchKernelState->graphNodeHandle, clonedGraphNodeDesc->phNodeHandle);
    auto graphNode = L0::GraphNode::fromHandle(nodeHandle);
    EXPECT_EQ(&closure.apiArgs.groupCounts, graphNode->getGroupCounts());
    EXPECT_EQ(&closure.apiArgs.groupSizes, graphNode->getGroupSizes());
    EXPECT_EQ(closure.indirectArgs.capturedKernel.get(), graphNode->getKernel());

    auto movedClosure = std::move(closure);
    EXPECT_EQ(capturedLaunchKernelState, movedClosure.indirectArgs.capturedLaunchKernelState.get());
    EXPECT_EQ(&capturedLaunchKernelState->graphNodeHandle, clonedGraphNodeDesc->phNodeHandle);
    EXPECT_EQ(&movedClosure.apiArgs.groupCounts, graphNode->getGroupCounts());
    EXPECT_EQ(&movedClosure.apiArgs.groupSizes, graphNode->getGroupSizes());
    EXPECT_EQ(movedClosure.indirectArgs.capturedKernel.get(), graphNode->getKernel());

    Closure<apiValue> assignedClosure(apiArgsWithoutGraphNode, storage);
    assignedClosure = std::move(movedClosure);
    EXPECT_EQ(capturedLaunchKernelState, assignedClosure.indirectArgs.capturedLaunchKernelState.get());
    EXPECT_EQ(&capturedLaunchKernelState->graphNodeHandle, clonedGraphNodeDesc->phNodeHandle);
    EXPECT_EQ(&assignedClosure.apiArgs.groupCounts, graphNode->getGroupCounts());
    EXPECT_EQ(&assignedClosure.apiArgs.groupSizes, graphNode->getGroupSizes());
    EXPECT_EQ(assignedClosure.indirectArgs.capturedKernel.get(), graphNode->getKernel());
}

TEST_F(GraphTestApiCaptureWithDevice, GivenAppendKernelWithParametersClosureWithoutCapturedLaunchKernelStateWhenMovedThenGraphNodeReferencesAreNotUpdated) {
    GraphsCleanupGuard graphCleanup;

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    L0::ClosureExternalStorage storage;
    storage.graphNodeCreationEnabled = true;

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};

    constexpr auto apiValue = CaptureApi::zeCommandListAppendLaunchKernelWithParameters;
    auto apiArgs = Closure<apiValue>::ApiArgs{
        .hCommandList = immCmdListHandle,
        .kernelHandle = kernel.toHandle(),
        .pGroupCounts = &groupCount,
        .pNext = &graphNodeDesc,
        .hSignalEvent = nullptr,
        .numWaitEvents = 0,
        .phWaitEvents = nullptr};

    Closure<apiValue> closure(apiArgs, storage);
    ASSERT_EQ(ZE_RESULT_SUCCESS, storage.lastResult);
    ASSERT_NE(nullptr, nodeHandle);
    auto graphNode = L0::GraphNode::fromHandle(nodeHandle);
    const auto *capturedGroupCounts = &closure.indirectArgs.groupCounts;
    closure.indirectArgs.capturedLaunchKernelState.reset();

    auto movedClosure = std::move(closure);
    EXPECT_EQ(capturedGroupCounts, graphNode->getGroupCounts());
}

TEST_F(GraphTestApiCaptureWithDevice, GivenAppendKernelWithArgumentsClosureWithoutCapturedLaunchKernelStateWhenMovedThenGraphNodeReferencesAreNotUpdated) {
    GraphsCleanupGuard graphCleanup;

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    L0::ClosureExternalStorage storage;
    storage.graphNodeCreationEnabled = true;

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;

    constexpr auto apiValue = CaptureApi::zeCommandListAppendLaunchKernelWithArguments;
    auto apiArgs = Closure<apiValue>::ApiArgs{
        .hCommandList = immCmdListHandle,
        .kernelHandle = kernel.toHandle(),
        .groupCounts = {1, 1, 1},
        .groupSizes = {1, 1, 1},
        .pArguments = nullptr,
        .pNext = &graphNodeDesc,
        .hSignalEvent = nullptr,
        .numWaitEvents = 0,
        .phWaitEvents = nullptr};

    Closure<apiValue> closure(apiArgs, storage);
    ASSERT_EQ(ZE_RESULT_SUCCESS, storage.lastResult);
    ASSERT_NE(nullptr, nodeHandle);
    auto graphNode = L0::GraphNode::fromHandle(nodeHandle);
    const auto *capturedGroupCounts = &closure.apiArgs.groupCounts;
    const auto *capturedGroupSizes = &closure.apiArgs.groupSizes;
    closure.indirectArgs.capturedLaunchKernelState.reset();

    auto movedClosure = std::move(closure);
    EXPECT_EQ(capturedGroupCounts, graphNode->getGroupCounts());
    EXPECT_EQ(capturedGroupSizes, graphNode->getGroupSizes());
}

TEST_F(GraphTestApiInstantiate, GivenInvalidSourceGraphThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = L0::zeCommandListInstantiateGraphExp(nullptr, &execGraphHandle, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraphHandle);
}

TEST_F(GraphTestApiInstantiate, GivenInvalidOutputGraphPlaceholderThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    L0::Graph srcGraph(&ctx, true, false);
    srcGraph.stopCapturing();
    auto srcGraphHandle = srcGraph.toHandle();

    auto err = L0::zeCommandListInstantiateGraphExp(srcGraphHandle, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenNonNullPNextThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    L0::Graph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    srcGraph.stopCapturing();

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = L0::zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, &ext);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraphHandle);
}

TEST_F(GraphTestApiInstantiate, GivenValidSourceGraphThenInstantiateReturnsValidExecutableGraph) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;

    L0::Graph srcGraph(&ctx, true, false);
    srcGraph.startCapturingFrom(*immCmdList, false);
    auto srcGraphHandle = srcGraph.toHandle();
    srcGraph.stopCapturing();

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = L0::zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, execGraphHandle);

    err = L0::zeExecutableGraphDestroyExp(execGraphHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenDumpGraphOnInstantiateFlagSetWhenInstantiatingThenGraphIsDumpedToFile) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;

    NEO::debugManager.flags.DumpGraphOnInstantiate.set(true);

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> fopenBackup(&NEO::IoFunctions::fopenPtr, NEO::IoFunctions::mockFopen);
    VariableBackup<decltype(NEO::IoFunctions::fwritePtr)> fwriteBackup(&NEO::IoFunctions::fwritePtr, NEO::IoFunctions::mockFwrite);
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> fcloseBackup(&NEO::IoFunctions::fclosePtr, NEO::IoFunctions::mockFclose);
    VariableBackup<FILE *> fopenReturnedBackup(&NEO::IoFunctions::mockFopenReturned, reinterpret_cast<FILE *>(0x1234));
    VariableBackup<size_t> fwriteReturnBackup(&NEO::IoFunctions::mockFwriteReturn, static_cast<size_t>(1));

    const auto fopenCalledBefore = NEO::IoFunctions::mockFopenCalled;

    L0::Graph srcGraph(&ctx, true, false);
    srcGraph.startCapturingFrom(*immCmdList, false);
    auto srcGraphHandle = srcGraph.toHandle();
    srcGraph.stopCapturing();

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = L0::zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(fopenCalledBefore + 1, NEO::IoFunctions::mockFopenCalled);

    err = L0::zeExecutableGraphDestroyExp(execGraphHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenDumpGraphOnInstantiateFlagNotSetWhenInstantiatingThenGraphIsNotDumped) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;

    NEO::debugManager.flags.DumpGraphOnInstantiate.set(false);

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> fopenBackup(&NEO::IoFunctions::fopenPtr, NEO::IoFunctions::mockFopen);
    VariableBackup<FILE *> fopenReturnedBackup(&NEO::IoFunctions::mockFopenReturned, reinterpret_cast<FILE *>(0x1234));

    const auto fopenCalledBefore = NEO::IoFunctions::mockFopenCalled;

    L0::Graph srcGraph(&ctx, true, false);
    srcGraph.startCapturingFrom(*immCmdList, false);
    auto srcGraphHandle = srcGraph.toHandle();
    srcGraph.stopCapturing();

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = L0::zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(fopenCalledBefore, NEO::IoFunctions::mockFopenCalled);

    err = L0::zeExecutableGraphDestroyExp(execGraphHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenInvalidExecutableGraphThenGraphDestroyReturnsError) {
    GraphsCleanupGuard graphCleanup;
    auto err = L0::zeExecutableGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenUnclosedGraphThenInstantiateFails) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    L0::Graph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = L0::zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, execGraphHandle);
}

TEST_F(GraphTestDebugApis, GivenNullGraphWhenIsGraphCaptureEnabledIsCalledThenReturnError) {
    GraphsCleanupGuard graphCleanup;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, L0::zeCommandListIsGraphCaptureEnabledExp(nullptr));
}

TEST_F(GraphTestDebugApis, GivenCommandListWithoutCaptureEnabledWhenIsGraphCaptureEnabledIsCalledThenReturnFalse) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    auto cmdListHandle = cmdlist.toHandle();
    EXPECT_EQ(ZE_RESULT_QUERY_FALSE_EXP, L0::zeCommandListIsGraphCaptureEnabledExp(cmdListHandle));
    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, L0::zeCommandListIsGraphCaptureEnabledExt(cmdListHandle));
}

TEST_F(GraphTestDebugApis, GivenCommandListWithCaptureEnabledWhenIsGraphCaptureEnabledIsCalledThenReturnTrue) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();

    Graph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);

    EXPECT_EQ(ZE_RESULT_QUERY_TRUE_EXP, L0::zeCommandListIsGraphCaptureEnabledExp(cmdListHandle));
    EXPECT_EQ(ZE_RESULT_QUERY_TRUE, L0::zeCommandListIsGraphCaptureEnabledExt(cmdListHandle));

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 0U, nullptr);

    srcGraph.stopCapturing();
    cmdlist.setGraphCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_QUERY_FALSE_EXP, L0::zeCommandListIsGraphCaptureEnabledExp(cmdListHandle));
    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, L0::zeCommandListIsGraphCaptureEnabledExt(cmdListHandle));
}

TEST_F(GraphTestDebugApis, GivenNullGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeGraphIsEmptyExp(nullptr));
}

TEST_F(GraphTestDebugApis, GivenInvalidGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, forkEvent);
    srcSubGraph->stopCapturing();
    subCmdlist.setGraphCaptureTarget(nullptr);
    srcGraph.stopCapturing();
    cmdlist.setGraphCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GRAPH_EXP, L0::zeGraphIsEmptyExp(srcGraphHandle));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GRAPH, L0::zeGraphIsEmptyExt(srcGraphHandle));
}

TEST_F(GraphTestDebugApis, GivenEmptyGraphWhenGraphIsEmptyIsCalledThenTrue) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Graph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();

    EXPECT_EQ(ZE_RESULT_QUERY_TRUE_EXP, L0::zeGraphIsEmptyExp(srcGraphHandle));
    EXPECT_EQ(ZE_RESULT_QUERY_TRUE, L0::zeGraphIsEmptyExt(srcGraphHandle));
}

TEST_F(GraphTestDebugApis, GivenNonEmptyGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    auto cmdListHandle = cmdlist.toHandle();
    cmdlist.device = this->device;

    Graph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 0U, nullptr);

    srcGraph.stopCapturing();
    cmdlist.setGraphCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_QUERY_FALSE_EXP, L0::zeGraphIsEmptyExp(srcGraphHandle));
    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, L0::zeGraphIsEmptyExt(srcGraphHandle));
}

TEST_F(GraphTestApiSubmit, GivenNonNullPNextThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraph srcGraph(&ctx, true, false);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    auto execGraphHandle = execGraph.toHandle();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    execGraph.instantiateFrom(srcGraph);

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = L0::zeCommandListAppendGraphExp(cmdListHandle, execGraphHandle, &ext, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiSubmit, GivenInvalidCmdListThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph srcGraph(&ctx, true, false);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    auto execGraphHandle = execGraph.toHandle();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    execGraph.instantiateFrom(srcGraph);
    auto err = L0::zeCommandListAppendGraphExp(nullptr, execGraphHandle, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiSubmit, GivenInvalidGraphThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    auto cmdListHandle = cmdlist.toHandle();

    auto err = L0::zeCommandListAppendGraphExp(cmdListHandle, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiSubmit, GivenValidCmdListAndGraphThenGraphAppendReturnsSuccess) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraph srcGraph(&ctx, true, false);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    auto execGraphHandle = execGraph.toHandle();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    execGraph.instantiateFrom(srcGraph);

    auto err = L0::zeCommandListAppendGraphExp(cmdListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenCommandListInRecordStateThenCaptureCommandsInsteadOfExecutingThem) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    ContextStubMock otherCtx;
    Mock<Event> event;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    Mock<KernelImp> kernel2;
    kernel2.setModule(&module);
    ze_image_handle_t imgA = nullptr;
    ze_image_handle_t imgB = nullptr;
    ze_device_handle_t device = nullptr;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_external_semaphore_ext_handle_t sem = nullptr;
    ze_event_handle_t eventHandle = event.toHandle();
    ze_external_semaphore_signal_params_ext_t semSignalParams = {};
    ze_external_semaphore_wait_params_ext_t semWaitParams = {};
    ze_kernel_handle_t pKernelHandles[] = {kernel.toHandle(), kernel2.toHandle()};
    const uint32_t numKernels = 2U;
    const auto *pCountBuffer = &numKernels;

    uint64_t memA[16] = {};
    uint64_t memB[16] = {};
    const void *memRange = memA;
    size_t rangeSize = 4;
    ze_copy_region_t copyRegion = {};
    copyRegion.width = 1;
    copyRegion.height = 1;
    copyRegion.depth = 1;
    ze_image_region_t imgRegion = {};
    imgRegion.width = 1;
    imgRegion.height = 1;
    imgRegion.depth = 1;

    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    L0::Graph graph(&ctx, true, false);
    ze_graph_handle_t graphHandle = graph.toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, graphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(immCmdListHandle, memA, memB, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(immCmdListHandle, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEventsWithParameters(immCmdListHandle, nullptr, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWriteGlobalTimestamp(immCmdListHandle, memA, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryRangesBarrier(immCmdListHandle, 1, &rangeSize, &memRange, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFill(immCmdListHandle, memA, memB, 4, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyRegion(immCmdListHandle, memA, &copyRegion, 16, 16, memB, &copyRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyFromContext(immCmdListHandle, memA, &otherCtx, memB, 4, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopy(immCmdListHandle, imgA, imgB, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyRegion(immCmdListHandle, imgA, imgB, &imgRegion, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(immCmdListHandle, memA, imgA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(immCmdListHandle, imgA, memA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryPrefetch(immCmdListHandle, memA, 4));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemAdvise(immCmdListHandle, device, memA, 4, ZE_MEMORY_ADVICE_BIAS_CACHED));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(immCmdListHandle, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(immCmdListHandle, nullptr, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(immCmdListHandle, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendQueryKernelTimestamps(immCmdListHandle, 1, &eventHandle, memA, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalExternalSemaphoreExt(immCmdListHandle, 1, &sem, &semSignalParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitExternalSemaphoreExt(immCmdListHandle, 1, &sem, &semWaitParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemoryExt(immCmdListHandle, memA, imgA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemoryExt(immCmdListHandle, imgA, memA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(immCmdListHandle, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchCooperativeKernel(immCmdListHandle, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(immCmdListHandle, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchMultipleKernelsIndirect(immCmdListHandle, numKernels, pKernelHandles, pCountBuffer, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyWithParameters(immCmdListHandle, memA, memB, sizeof(memA), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFillWithParameters(immCmdListHandle, memA, memB, 4, sizeof(memA), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendHostFunction(immCmdListHandle, nullptr, nullptr, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &graphHandle, nullptr));

    ASSERT_EQ(32u, graph.getCapturedCommands().size());
    uint32_t i = 0;
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendWaitOnEvents, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendWaitOnEventsWithParameters, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendWriteGlobalTimestamp, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryRangesBarrier, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryFill, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopyRegion, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopyFromContext, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopy, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopyRegion, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopyToMemory, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopyFromMemory, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryPrefetch, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemAdvise, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendSignalEvent, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendSignalEventWithParameters, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendEventReset, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendQueryKernelTimestamps, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopyToMemoryExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopyFromMemoryExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernel, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchCooperativeKernel, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelIndirect, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelWithParameters, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelWithArguments, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopyWithParameters, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryFillWithParameters, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendHostFunction, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
}

TEST(GraphForks, GivenUnknownChildCommandlistThenJoinDoesNothing) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> childCmdlist;
    Mock<Event> joinEvent;
    Graph parent{&ctx, true, false};
    parent.tryJoinOnNextCommand(childCmdlist, joinEvent);
    EXPECT_TRUE(parent.getSubgraphs().empty());
}

TEST(GraphForks, GivenNullEventThenRecordHandleSignaleEventDoesNothing) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    MockGraph graph{&ctx, true, false};
    recordHandleSignalEventFromPreviousCommand(cmdlist, graph, nullptr);
    EXPECT_TRUE(graph.recordedSignals.empty());
}

TEST(GraphForks, GivenRegularEventDependencyThenCommandlistDoesNotStartRecording) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Graph graph{&ctx, true, false};

    Mock<Event> regularEvent;
    ze_event_handle_t regularEventHandle = regularEvent.toHandle();
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = zeCommandListAppendWaitOnEvents(cmdListHandle, 1, &regularEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, cmdlist.getGraphCaptureTarget());
}

TEST_F(GraphInstantiationValidation, WhenJoinIsResolvedThenResolvedJoinTracksParentAndChildCommandIds) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    auto joinEventHandle = joinEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *srcGraphPtr = &srcGraph;
    Graph *childGraph = nullptr;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &forkEventHandle);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();

    const auto &resolvedJoins = srcGraph.getResolvedJoins();
    ASSERT_EQ(1u, resolvedJoins.size());
    ASSERT_NE(nullptr, childGraph);

    auto joinInfoIt = resolvedJoins.find(childGraph);
    ASSERT_NE(resolvedJoins.end(), joinInfoIt);
    EXPECT_EQ(0u, joinInfoIt->second.forkSignalCommandId);
    EXPECT_EQ(1u, joinInfoIt->second.joinWaitCommandId);
    EXPECT_EQ(0u, joinInfoIt->second.joinSignalCommandId);
    EXPECT_EQ(forkEventHandle, joinInfoIt->second.forkEvent);
    EXPECT_EQ(joinEventHandle, joinInfoIt->second.joinEvent);
    EXPECT_EQ(childGraph, joinInfoIt->second.forkDestiny);
}

TEST_F(GraphInstantiationValidation, WhenSingleForkEventIsAwaitedByMultipleCommandListsThenJoinsOfAllForkedCommandListsAreResolved) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext firstChildCmdlist{&ctx};
    firstChildCmdlist.device = this->device;
    MockGraphCmdListWithContext secondChildCmdlist{&ctx};
    secondChildCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> firstJoinEvent;
    auto firstJoinEventHandle = firstJoinEvent.toHandle();
    Mock<Event> secondJoinEvent;
    auto secondJoinEventHandle = secondJoinEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *srcGraphPtr = &srcGraph;
    Graph *firstChildGraph = nullptr;
    Graph *secondChildGraph = nullptr;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(firstChildCmdlist, firstChildGraph, nullptr, &firstChildCmdlist, firstJoinEventHandle, 1U, &forkEventHandle);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(secondChildCmdlist, secondChildGraph, nullptr, &secondChildCmdlist, secondJoinEventHandle, 1U, &forkEventHandle);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &firstJoinEventHandle);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &secondJoinEventHandle);

    srcGraph.stopCapturing();

    EXPECT_TRUE(srcGraph.getUnjoinedForks().empty());
    ASSERT_NE(nullptr, firstChildGraph);
    ASSERT_NE(nullptr, secondChildGraph);

    const auto &resolvedJoins = srcGraph.getResolvedJoins();
    ASSERT_EQ(2u, resolvedJoins.size());

    auto firstJoinInfoIt = resolvedJoins.find(firstChildGraph);
    ASSERT_NE(resolvedJoins.end(), firstJoinInfoIt);
    EXPECT_EQ(forkEventHandle, firstJoinInfoIt->second.forkEvent);
    EXPECT_EQ(firstJoinEventHandle, firstJoinInfoIt->second.joinEvent);
    EXPECT_EQ(1u, firstJoinInfoIt->second.joinWaitCommandId);

    auto secondJoinInfoIt = resolvedJoins.find(secondChildGraph);
    ASSERT_NE(resolvedJoins.end(), secondJoinInfoIt);
    EXPECT_EQ(forkEventHandle, secondJoinInfoIt->second.forkEvent);
    EXPECT_EQ(secondJoinEventHandle, secondJoinInfoIt->second.joinEvent);
    EXPECT_EQ(2u, secondJoinInfoIt->second.joinWaitCommandId);
}

TEST_F(GraphInstantiationValidation, WhenChildHasAllowedPostJoinCommandThenResolvedJoinStillPointsToJoinSignalCommand) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    auto joinEventHandle = joinEvent.toHandle();
    Mock<Event> postJoinSignalEvent;
    auto postJoinSignalEventHandle = postJoinSignalEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *srcGraphPtr = &srcGraph;
    Graph *childGraph = nullptr;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &forkEventHandle);
    L0::captureCommand<CaptureApi::zeCommandListAppendSignalEvent>(childCmdlist, childGraph, nullptr, &childCmdlist, postJoinSignalEventHandle);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();

    const auto &resolvedJoins = srcGraph.getResolvedJoins();
    ASSERT_EQ(1u, resolvedJoins.size());
    ASSERT_NE(nullptr, childGraph);

    auto joinInfoIt = resolvedJoins.find(childGraph);
    ASSERT_NE(resolvedJoins.end(), joinInfoIt);
    EXPECT_EQ(0u, joinInfoIt->second.joinSignalCommandId);
}

TEST_F(GraphInstantiation, GivenSourceGraphThenExecutableIsInstantiatedProperly) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    Mock<CommandList> subCmdlist;
    subCmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    subCmdlist.device = this->device;
    Mock<Event> signalEvents[3];
    ze_event_handle_t signalEventsHandles[3]{signalEvents[0].toHandle(), signalEvents[1].toHandle(), signalEvents[2].toHandle()};
    Mock<Event> waitEvents[3];
    ze_event_handle_t waitEventsList[3] = {waitEvents[0].toHandle(), waitEvents[1].toHandle(), waitEvents[2].toHandle()};
    uint64_t memA[16] = {};
    uint64_t memB[16] = {};

    GraphInstatiateSettings instantiateAsMonolithic(nullptr, true);
    ASSERT_EQ(GraphInstatiateSettings::ForkPolicyMonolythicLevels, instantiateAsMonolithic.forkPolicy);
    {
        ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
        auto *graphCmdList = ctx.cmdListsToReturn[0];
        MockGraph srcGraph(&ctx, true, false);
        srcGraph.captureTargetDesc.hDevice = device->toHandle();
        cmdlist.setGraphCaptureTarget(&srcGraph);
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, signalEvents[0].toHandle(), 2U, waitEventsList);
        cmdlist.capture<CaptureApi::zeCommandListAppendMemoryCopy>(cmdListHandle, memA, memB, sizeof(memA), signalEvents[1].toHandle(), 3U, waitEventsList);
        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.isSubGraph());

        ASSERT_EQ(2U, srcGraph.getCapturedCommands().size());
        EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[0].index()));
        EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[1].index()));

        ExecutableGraph execGraph;
        execGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);
        EXPECT_FALSE(execGraph.isSubGraph());
        EXPECT_FALSE(execGraph.empty());
        EXPECT_TRUE(execGraph.getSubgraphs().empty());
        EXPECT_EQ(1U, graphCmdList->appendBarrierCalled);
        EXPECT_EQ(1U, graphCmdList->appendMemoryCopyCalled);
    }

    {
        ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
        ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
        auto *graphCmdList = ctx.cmdListsToReturn[0];
        MockGraph srcGraph(&ctx, true, false);
        srcGraph.captureTargetDesc.hDevice = device->toHandle();
        cmdlist.setGraphCaptureTarget(&srcGraph);
        srcGraph.startCapturingFrom(cmdlist, false);
        Graph *srcSubGraph = nullptr;
        cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, signalEvents[0].toHandle(), 0U, nullptr);
        subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&subCmdlist, signalEvents[2].toHandle(), 1U, &signalEventsHandles[0]);
        cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &signalEventsHandles[2]);
        srcSubGraph = subCmdlist.getGraphCaptureTarget();
        srcGraph.stopCapturing();

        ASSERT_EQ(1U, srcGraph.getSubgraphs().size());
        EXPECT_TRUE(srcGraph.getSubgraphs()[0]->isSubGraph());
        EXPECT_FALSE(srcGraph.getSubgraphs()[0]->empty());

        static_cast<MockGraph *>(srcSubGraph)->captureTargetDesc.hDevice = device->toHandle();

        ExecutableGraph execMultiGraph;
        execMultiGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);

        EXPECT_FALSE(execMultiGraph.isSubGraph());
        EXPECT_EQ(2U, graphCmdList->appendBarrierCalled);
        EXPECT_EQ(0U, graphCmdList->appendMemoryCopyCalled);
        ASSERT_EQ(1U, execMultiGraph.getSubgraphs().size());
        EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->isSubGraph());
        EXPECT_FALSE(execMultiGraph.getSubgraphs()[0]->empty());
    }
}

TEST_F(GraphInstantiation, GivenSourceGraphWithMetricCommandsThenExecutableReplaysMetricCalls) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();

    Mock<Event> signalEvent;
    Mock<Event> waitEvent;
    ze_event_handle_t waitEvents[] = {waitEvent.toHandle()};

    auto metricStreamerHandle = reinterpret_cast<zet_metric_streamer_handle_t>(0x1001);
    auto metricQueryHandle = reinterpret_cast<zet_metric_query_handle_t>(0x1002);

    GraphInstatiateSettings instantiateAsMonolithic(nullptr, true);

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
    auto *graphCmdList = ctx.cmdListsToReturn[0];
    MockGraph srcGraph(&ctx, true, false);
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);

    cmdlist.capture<CaptureApi::zetCommandListAppendMetricStreamerMarker>(cmdListHandle, metricStreamerHandle, 7U);
    cmdlist.capture<CaptureApi::zetCommandListAppendMetricQueryBegin>(cmdListHandle, metricQueryHandle);
    cmdlist.capture<CaptureApi::zetCommandListAppendMetricQueryEnd>(cmdListHandle, metricQueryHandle, signalEvent.toHandle(), 1U, waitEvents);
    cmdlist.capture<CaptureApi::zetCommandListAppendMetricMemoryBarrier>(cmdListHandle);

    srcGraph.stopCapturing();

    ASSERT_EQ(4U, srcGraph.getCapturedCommands().size());
    EXPECT_EQ(CaptureApi::zetCommandListAppendMetricStreamerMarker, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[0].index()));
    EXPECT_EQ(CaptureApi::zetCommandListAppendMetricQueryBegin, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[1].index()));
    EXPECT_EQ(CaptureApi::zetCommandListAppendMetricQueryEnd, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[2].index()));
    EXPECT_EQ(CaptureApi::zetCommandListAppendMetricMemoryBarrier, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[3].index()));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);

    EXPECT_EQ(1U, graphCmdList->appendMetricStreamerMarkerCalled);
    EXPECT_EQ(1U, graphCmdList->appendMetricQueryBeginCalled);
    EXPECT_EQ(1U, graphCmdList->appendMetricQueryEndCalled);
    EXPECT_EQ(1U, graphCmdList->appendMetricMemoryBarrierCalled);
}

TEST_F(GraphInstantiation, GivenSourceGraphThenExecutableIsInstantiatedWithPreservedOrderOfForkAndJoinCommands) {
    GraphsCleanupGuard graphCleanup;

    struct CmdInfo {
        ze_event_handle_t waitEvent = {};
        ze_event_handle_t signalEvent = {};

        bool operator==(const CmdInfo &rhs) const {
            return (this->waitEvent == rhs.waitEvent) && (this->signalEvent == rhs.signalEvent);
        }
    };

    struct MockCmdListCheckSequence : Mock<CommandList> {
        using WhiteBox<::L0::CommandList>::cmdListType;

        std::vector<CmdInfo> &sequenceContainer;
        MockCmdListCheckSequence(std::vector<CmdInfo> &sequenceContainer) : sequenceContainer(sequenceContainer) {
            cmdListType = ::L0::CommandList::CommandListType::typeImmediate;
        }

        ze_result_t appendBarrier(ze_event_handle_t hSignalEvent,
                                  uint32_t numWaitEvents,
                                  ze_event_handle_t *phWaitEvents, CmdListWaitEventParameters &waitEventsParameters, CmdListSignalEventParameters &signalEventParameters) override {
            sequenceContainer.push_back(CmdInfo{numWaitEvents ? phWaitEvents[0] : nullptr, hSignalEvent});
            return ZE_RESULT_SUCCESS;
        }
    };

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.cmdListType = L0::CommandList::CommandListType::typeImmediate;
    subCmdlist.device = this->device;
    auto subCmdListHandle = subCmdlist.toHandle();
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdListHandle, &joinEvent, 1U, &forkEventHandle);

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    for (auto &srcSubgraph : srcGraph.getSubgraphs()) {
        static_cast<MockGraph *>(srcSubgraph)->captureTargetDesc.hDevice = device->toHandle();
    }

    std::vector<CmdInfo> instantiatedSequence;

    ctx.cmdListsToReturn.push_back(new MockCmdListCheckSequence{instantiatedSequence});
    ctx.cmdListsToReturn.push_back(new MockCmdListCheckSequence{instantiatedSequence});
    ctx.cmdListsToReturn.push_back(new MockCmdListCheckSequence{instantiatedSequence});

    MockExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, {});

    CmdInfo expectedSequence[3] = {
        {nullptr, forkEventHandle},
        {forkEventHandle, &joinEvent},
        {&joinEvent, nullptr},
    };

    ASSERT_EQ(3U, instantiatedSequence.size());
    EXPECT_EQ(expectedSequence[0], instantiatedSequence[0]);
    EXPECT_EQ(expectedSequence[1], instantiatedSequence[1]);
    EXPECT_EQ(expectedSequence[2], instantiatedSequence[2]);
}

TEST_F(GraphInstantiation, GivenForkedGraphWhenVisitingWithReappendTargetCommandListThenOnlyRootLevelCommandsAreReappendedToTarget) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    auto subCmdListHandle = subCmdlist.toHandle();
    MockGraphCmdListWithContext targetCmdlist{&ctx};
    targetCmdlist.device = this->device;
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdListHandle, joinEventHandle, 1U, &forkEventHandle);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &joinEventHandle);
    srcGraph.stopCapturing();
    cmdlist.setGraphCaptureTarget(nullptr);
    subCmdlist.setGraphCaptureTarget(nullptr);
    ASSERT_EQ(1U, srcGraph.getSubgraphs().size());

    for (bool explicitTarget : {false, true}) {
        cmdlist.appendBarrierCalled = 0;
        subCmdlist.appendBarrierCalled = 0;
        targetCmdlist.appendBarrierCalled = 0;

        ze_command_visit_ext_desc_t desc{ZE_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC};
        desc.defaultOp = ZE_COMMAND_VISIT_EXT_DEFAULT_OP_REAPPEND;
        desc.hReappendTargetCommandList = explicitTarget ? targetCmdlist.toHandle() : nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, srcGraph.visit(&desc));

        EXPECT_EQ(explicitTarget ? 0U : 2U, cmdlist.appendBarrierCalled);
        EXPECT_EQ(explicitTarget ? 2U : 0U, targetCmdlist.appendBarrierCalled);
        EXPECT_EQ(1U, subCmdlist.appendBarrierCalled);
    }
}

TEST_F(GraphInstantiation, GivenSourceGraphWhenPolicyIsSetToInterleaveThenExecutableInterleavesParentAndChildCommandListBasedOnForks) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, &joinEvent, 1U, &forkEventHandle);

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    for (auto &srcSubgraph : srcGraph.getSubgraphs()) {
        static_cast<MockGraph *>(srcSubgraph)->captureTargetDesc.hDevice = device->toHandle();
    }

    {
        GraphInstatiateSettings instantiateAsMonolithic;
        instantiateAsMonolithic.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;

        MockExecutableGraph execMultiGraph;
        execMultiGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);
        EXPECT_EQ(1U, execMultiGraph.myOrderedSegments.size()); // parent0 -> child
    }

    {
        GraphInstatiateSettings instantiateAsInterleaved(nullptr, false);
        instantiateAsInterleaved.forkPolicy = GraphInstatiateSettings::ForkPolicySplitLevels;

        MockExecutableGraph execMultiGraph;
        execMultiGraph.instantiateFrom(srcGraph, instantiateAsInterleaved);
        EXPECT_EQ(2U, execMultiGraph.myOrderedSegments.size()); // parent0 -> child -> parent1
    }

    {
        GraphInstatiateSettings instantiateAsFlat(nullptr, false);
        ASSERT_EQ(GraphInstatiateSettings::ForkPolicyFlat, instantiateAsFlat.forkPolicy);

        MockExecutableGraph execMultiGraph;
        execMultiGraph.instantiateFrom(srcGraph, instantiateAsFlat);
        EXPECT_EQ(1U, execMultiGraph.myOrderedSegments.size()); //  parent0 -> child -> parent1 in a single commandlist
    }
}

TEST_F(GraphInstantiation, GivenExecGraphBuilderWhenFlatCommandListSetThenGetterReturnsSameValue) {
    GraphsCleanupGuard graphCleanup;

    ContextStubMock ctx;
    MockGraph srcGraph(&ctx, true, false);
    MockExecutableGraph execGraph;
    ExecGraphBuilder builder(srcGraph, execGraph);

    EXPECT_EQ(nullptr, builder.getFlatCommandList());

    Mock<CommandList> flatCmdList;
    builder.setFlatCommandList(&flatCmdList);

    EXPECT_EQ(&flatCmdList, builder.getFlatCommandList());
}

TEST_F(GraphInstantiation, GivenFlatPolicyWhenInstantiatingForkedGraphThenSharedCommandListIsClosedOnce) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, &joinEvent, 1U, &forkEventHandle);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    for (auto &srcSubgraph : srcGraph.getSubgraphs()) {
        static_cast<MockGraph *>(srcSubgraph)->captureTargetDesc.hDevice = device->toHandle();
    }

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
    auto *sharedFlatCmdList = ctx.cmdListsToReturn[0];

    GraphInstatiateSettings instantiateAsFlat(nullptr, false);
    ASSERT_EQ(GraphInstatiateSettings::ForkPolicyFlat, instantiateAsFlat.forkPolicy);

    MockExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsFlat);

    EXPECT_EQ(1U, execMultiGraph.myOrderedSegments.size());
    EXPECT_EQ(1U, sharedFlatCmdList->closeCalled);
}

TEST_F(GraphInstantiation, GivenGraphWithSiblingAndNestedForksWhenInstantiatingThenAllExecutableSubgraphsShareNodeCommandsOfRootExecutableGraph) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext firstChildCmdlist{&ctx};
    firstChildCmdlist.device = this->device;
    MockGraphCmdListWithContext secondChildCmdlist{&ctx};
    secondChildCmdlist.device = this->device;
    MockGraphCmdListWithContext nestedChildCmdlist{&ctx};
    nestedChildCmdlist.device = this->device;
    Mock<Event> firstForkEvent;
    Mock<Event> secondForkEvent;
    Mock<Event> nestedForkEvent;
    Mock<Event> firstJoinEvent;
    Mock<Event> secondJoinEvent;
    Mock<Event> nestedJoinEvent;
    ze_event_handle_t firstForkEventHandle = firstForkEvent.toHandle();
    ze_event_handle_t secondForkEventHandle = secondForkEvent.toHandle();
    ze_event_handle_t nestedForkEventHandle = nestedForkEvent.toHandle();
    ze_event_handle_t firstJoinEventHandle = firstJoinEvent.toHandle();
    ze_event_handle_t secondJoinEventHandle = secondJoinEvent.toHandle();
    ze_event_handle_t nestedJoinEventHandle = nestedJoinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, firstForkEventHandle, 0U, nullptr);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, secondForkEventHandle, 0U, nullptr);
    firstChildCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(firstChildCmdlist.toHandle(), nestedForkEventHandle, 1U, &firstForkEventHandle);
    nestedChildCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(nestedChildCmdlist.toHandle(), nestedJoinEventHandle, 1U, &nestedForkEventHandle);
    firstChildCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(firstChildCmdlist.toHandle(), firstJoinEventHandle, 1U, &nestedJoinEventHandle);
    secondChildCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(secondChildCmdlist.toHandle(), secondJoinEventHandle, 1U, &secondForkEventHandle);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &firstJoinEventHandle);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &secondJoinEventHandle);
    srcGraph.stopCapturing();

    std::function<void(Graph &)> setCaptureTargetDevice = [&](Graph &graph) {
        static_cast<MockGraph &>(graph).captureTargetDesc.hDevice = device->toHandle();
        for (auto *srcSubgraph : graph.getSubgraphs()) {
            setCaptureTargetDevice(*srcSubgraph);
        }
    };
    setCaptureTargetDevice(srcGraph);

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());

    GraphInstatiateSettings instantiateAsFlat(nullptr, false);
    MockExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsFlat);

    auto rootNodeCommands = execMultiGraph.getNodeCommands().get();
    ASSERT_NE(nullptr, rootNodeCommands);
    uint32_t visitedSubgraphs = 0U;
    std::function<void(ExecutableGraph &)> expectSharedNodeCommands = [&](ExecutableGraph &execGraph) {
        for (auto &execSubgraph : execGraph.getSubgraphs()) {
            ++visitedSubgraphs;
            EXPECT_EQ(rootNodeCommands, execSubgraph->getNodeCommands().get());
            expectSharedNodeCommands(*execSubgraph);
        }
    };
    expectSharedNodeCommands(execMultiGraph);
    EXPECT_EQ(3U, visitedSubgraphs);
}

TEST_F(GraphInstantiation, GivenFlatPolicyWhenInstantiatingForkedGraphThenCommandBufferSizeEstimateCoversWholeTree) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, &joinEvent, 1U, &forkEventHandle);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    for (auto &srcSubgraph : srcGraph.getSubgraphs()) {
        static_cast<MockGraph *>(srcSubgraph)->captureTargetDesc.hDevice = device->toHandle();
    }

    uint32_t expectedEstimate = static_cast<uint32_t>(srcGraph.getCapturedCommands().size());
    for (auto *srcSubgraph : srcGraph.getSubgraphs()) {
        expectedEstimate += static_cast<uint32_t>(srcSubgraph->getCapturedCommands().size());
    }
    EXPECT_LT(srcGraph.getCapturedCommands().size(), expectedEstimate);

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());

    GraphInstatiateSettings instantiateAsFlat(nullptr, false);
    ASSERT_EQ(GraphInstatiateSettings::ForkPolicyFlat, instantiateAsFlat.forkPolicy);

    MockExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsFlat);

    ASSERT_EQ(1U, ctx.estimatedNumberOfCommandsPerCreate.size());
    EXPECT_EQ(expectedEstimate, ctx.estimatedNumberOfCommandsPerCreate[0]);
}

TEST_F(GraphInstantiation, GivenMonolithicPolicyWhenInstantiatingForkedGraphThenEachCommandListEstimateMatchesItsSubgraphCommandCount) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, &joinEvent, 1U, &forkEventHandle);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    for (auto &srcSubgraph : srcGraph.getSubgraphs()) {
        static_cast<MockGraph *>(srcSubgraph)->captureTargetDesc.hDevice = device->toHandle();
    }

    ASSERT_EQ(1U, srcGraph.getSubgraphs().size());
    auto parentCommands = static_cast<uint32_t>(srcGraph.getCapturedCommands().size());
    auto childCommands = static_cast<uint32_t>(srcGraph.getSubgraphs()[0]->getCapturedCommands().size());
    ASSERT_LT(childCommands, parentCommands);

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());

    GraphInstatiateSettings instantiateAsMonolithic(nullptr, true);
    ASSERT_EQ(GraphInstatiateSettings::ForkPolicyMonolythicLevels, instantiateAsMonolithic.forkPolicy);

    MockExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);

    ASSERT_EQ(2U, ctx.estimatedNumberOfCommandsPerCreate.size());
    EXPECT_EQ(parentCommands, ctx.estimatedNumberOfCommandsPerCreate[0]);
    EXPECT_EQ(childCommands, ctx.estimatedNumberOfCommandsPerCreate[1]);
}

TEST_F(GraphInstantiation, GivenSplitPolicyWhenInstantiatingForkedGraphThenPerSegmentEstimatesSumToTheWholeTree) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    subCmdlist.device = this->device;
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, &joinEvent, 1U, &forkEventHandle);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    for (auto &srcSubgraph : srcGraph.getSubgraphs()) {
        static_cast<MockGraph *>(srcSubgraph)->captureTargetDesc.hDevice = device->toHandle();
    }

    uint32_t parentCommands = static_cast<uint32_t>(srcGraph.getCapturedCommands().size());
    uint32_t totalTreeCommands = parentCommands;
    for (auto *srcSubgraph : srcGraph.getSubgraphs()) {
        totalTreeCommands += static_cast<uint32_t>(srcSubgraph->getCapturedCommands().size());
    }

    for (uint32_t i = 0; i < totalTreeCommands; i++) {
        ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
    }

    GraphInstatiateSettings instantiateAsSplit(nullptr, false);
    instantiateAsSplit.forkPolicy = GraphInstatiateSettings::ForkPolicySplitLevels;

    MockExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsSplit);

    ASSERT_GT(ctx.estimatedNumberOfCommandsPerCreate.size(), 1u);
    uint32_t sumOfEstimates = 0;
    for (auto estimate : ctx.estimatedNumberOfCommandsPerCreate) {
        sumOfEstimates += estimate;
        EXPECT_LT(estimate, parentCommands);
    }
    EXPECT_EQ(totalTreeCommands, sumOfEstimates);
}

TEST_F(GraphInstantiation, GivenSingleLevelGraphWhenInstantiatingWithAnyForkPolicyThenCommandBufferEstimateMatchesRecordedCommandCount) {
    GraphsCleanupGuard graphCleanup;

    Mock<Event> signalEvent;
    uint64_t memA[16] = {};
    uint64_t memB[16] = {};

    GraphInstatiateSettings::ForkPolicy policies[] = {
        GraphInstatiateSettings::ForkPolicyMonolythicLevels,
        GraphInstatiateSettings::ForkPolicySplitLevels,
        GraphInstatiateSettings::ForkPolicyFlat};

    for (auto policy : policies) {
        MockGraphContextReturningSpecificCmdList ctx;
        MockGraphCmdListWithContext cmdlist{&ctx};
        cmdlist.device = this->device;
        auto cmdListHandle = cmdlist.toHandle();

        MockGraph srcGraph(&ctx, true, false);
        srcGraph.captureTargetDesc.hDevice = device->toHandle();
        cmdlist.setGraphCaptureTarget(&srcGraph);
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, signalEvent.toHandle(), 0U, nullptr);
        cmdlist.capture<CaptureApi::zeCommandListAppendMemoryCopy>(cmdListHandle, memA, memB, sizeof(memA), nullptr, 0U, nullptr);
        srcGraph.stopCapturing();

        ASSERT_EQ(2U, srcGraph.getCapturedCommands().size());
        ASSERT_TRUE(srcGraph.getSubgraphs().empty());

        ctx.cmdListsToReturn.push_back(new Mock<CommandList>());

        GraphInstatiateSettings settings;
        settings.forkPolicy = policy;

        MockExecutableGraph execGraph;
        execGraph.instantiateFrom(srcGraph, settings);

        ASSERT_EQ(1U, ctx.estimatedNumberOfCommandsPerCreate.size());
        EXPECT_EQ(2U, ctx.estimatedNumberOfCommandsPerCreate[0]);
    }
}

TEST_F(GraphInstantiationValidation, WhenGraphIsStillCapturingThenItIsNotValidForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    Graph srcGraph(&ctx, true, false);
    EXPECT_FALSE(srcGraph.validForInstantiation());
    srcGraph.stopCapturing();
    EXPECT_TRUE(srcGraph.validForInstantiation());
}

TEST_F(GraphInstantiationValidation, WhenGraphHasUnjoinedForksThenItIsNotValidForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    MockGraphCmdListWithContext grandChildCmdlist{&ctx};
    grandChildCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    auto joinEventHandle = joinEvent.toHandle();
    { // correct graph (0->1->0->close)
        Graph srcGraph(&ctx, true, false);
        Graph *srcGraphPtr = &srcGraph;

        // lvl 0
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.setGraphCaptureTarget(&srcGraph);
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

        { // lvl 1
            Graph *childGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &forkEventHandle);
        }
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

        srcGraph.stopCapturing();
        EXPECT_TRUE(srcGraph.validForInstantiation());
    }

    { // missing join lvl (0->1 0->close)
        Graph srcGraph(&ctx, true, false);
        Graph *srcGraphPtr = &srcGraph;

        // lvl 0
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.setGraphCaptureTarget(&srcGraph);
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

        { // lvl 1
            Graph *childGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &forkEventHandle);
        }
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 0U, nullptr);

        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.validForInstantiation());
    }

    { // unjoined work (false join on 1->0)   (0->1+! ->0->close)
        Graph srcGraph(&ctx, true, false);
        Graph *srcGraphPtr = &srcGraph;

        // lvl 0
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.setGraphCaptureTarget(&srcGraph);
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

        { // lvl 1
            Graph *childGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &forkEventHandle);
            L0::captureCommand<CaptureApi::zeCommandListAppendMemoryPrefetch>(grandChildCmdlist, childGraph, nullptr, &childCmdlist, nullptr, 0U);
        }
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.validForInstantiation());
    }
}

TEST_F(GraphInstantiationValidation, WhenGraphHasOnlyAllowedPostJoinCommandsThenItIsValidForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    auto joinEventHandle = joinEvent.toHandle();
    Mock<Event> postJoinSignalEvent;
    auto postJoinSignalEventHandle = postJoinSignalEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *srcGraphPtr = &srcGraph;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

    {
        Graph *childGraph = nullptr;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &forkEventHandle);
        L0::captureCommand<CaptureApi::zeCommandListAppendSignalEvent>(childCmdlist, childGraph, nullptr, &childCmdlist, postJoinSignalEventHandle);
    }
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    EXPECT_TRUE(srcGraph.validForInstantiation());
}

TEST_F(GraphInstantiationValidation, WhenSubGraphsAreNotValidForInstantiationThenWholeGraphIsNotReadyForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    MockGraphCmdListWithContext grandChildCmdlist{&ctx};
    grandChildCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> forkEventLvl2;
    auto forkEventHandleLvl2 = forkEventLvl2.toHandle();
    Mock<Event> joinEvent;
    Mock<Event> joinEventLvl2;
    auto joinEventHandle = joinEvent.toHandle();
    auto joinEventHandleLvl2 = joinEventLvl2.toHandle();
    { // correct graph (0->1->2->1->0->close)
        Graph srcGraph(&ctx, true, false);
        Graph *srcGraphPtr = &srcGraph;

        // lvl 0
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.setGraphCaptureTarget(&srcGraph);
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

        { // lvl 1
            Graph *childGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, forkEventHandleLvl2, 1U, &forkEventHandle);
            {
                Graph *grandChildGraph = nullptr;
                // lvl 2
                L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildGraph, nullptr, &childCmdlist, joinEventHandleLvl2, 1U, &forkEventHandleLvl2);
            }
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &joinEventHandleLvl2);
        }
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

        srcGraph.stopCapturing();
        EXPECT_TRUE(srcGraph.validForInstantiation());
    }

    { // missing join lvl 2->1 (0->1->2 1->0->close)
        Graph srcGraph(&ctx, true, false);
        Graph *srcGraphPtr = &srcGraph;

        // lvl 0
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.setGraphCaptureTarget(&srcGraph);
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

        { // lvl 1
            Graph *childGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, forkEventHandleLvl2, 1U, &forkEventHandle);
            {
                Graph *grandChildGraph = nullptr;
                // lvl 2
                L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildGraph, nullptr, &grandChildCmdlist, nullptr, 1U, &forkEventHandleLvl2);
            }
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &grandChildCmdlist, joinEventHandle, 0U, nullptr);
        }
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.validForInstantiation());
    }

    { // unjoined work (false join on 2->1)   (0->1->2->2+!->1->0->close)
        Graph srcGraph(&ctx, true, false);
        Graph *srcGraphPtr = &srcGraph;

        // lvl 0
        srcGraph.startCapturingFrom(cmdlist, false);
        cmdlist.setGraphCaptureTarget(&srcGraph);
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

        { // lvl 1
            Graph *childGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, forkEventHandleLvl2, 1U, &forkEventHandle);
            {
                Graph *grandChildGraph = nullptr;
                // lvl 2
                L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildGraph, nullptr, &grandChildCmdlist, joinEventHandleLvl2, 1U, &forkEventHandleLvl2);
                L0::captureCommand<CaptureApi::zeCommandListAppendMemoryPrefetch>(grandChildCmdlist, grandChildGraph, nullptr, &grandChildCmdlist, nullptr, 0U);
            }
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &joinEventHandleLvl2);
        }
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.validForInstantiation());
    }
}

TEST_F(GraphInstantiationValidation, WhenSubGraphHasOnlyAllowedPostJoinCommandsThenWholeGraphIsValidForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    MockGraphCmdListWithContext grandChildCmdlist{&ctx};
    grandChildCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> forkEventLvl2;
    auto forkEventHandleLvl2 = forkEventLvl2.toHandle();
    Mock<Event> joinEvent;
    Mock<Event> joinEventLvl2;
    auto joinEventHandle = joinEvent.toHandle();
    auto joinEventHandleLvl2 = joinEventLvl2.toHandle();
    Mock<Event> postJoinSignalEvent;
    auto postJoinSignalEventHandle = postJoinSignalEvent.toHandle();

    Graph srcGraph(&ctx, true, false);
    Graph *srcGraphPtr = &srcGraph;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

    {
        Graph *childGraph = nullptr;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, forkEventHandleLvl2, 1U, &forkEventHandle);
        {
            Graph *grandChildGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildGraph, nullptr, &grandChildCmdlist, joinEventHandleLvl2, 1U, &forkEventHandleLvl2);
            L0::captureCommand<CaptureApi::zeCommandListAppendSignalEvent>(grandChildCmdlist, grandChildGraph, nullptr, &grandChildCmdlist, postJoinSignalEventHandle);
        }
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &joinEventHandleLvl2);
    }
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);

    srcGraph.stopCapturing();
    EXPECT_TRUE(srcGraph.validForInstantiation());
}

TEST_F(GraphTestInstantiationTest, WhenInstantiatingGraphThenBakeCommandsIntoCommandlists) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphContextReturningSpecificCmdList otherCtx;
    Mock<Event> event;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    Mock<KernelImp> kernel2;
    kernel2.setModule(&module);
    ze_image_handle_t imgA = nullptr;
    ze_image_handle_t imgB = nullptr;
    zes_device_handle_t device = nullptr;
    ze_external_semaphore_ext_handle_t sem = nullptr;
    ze_event_handle_t eventHandle = event.toHandle();
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_kernel_handle_t pKernelHandles[] = {kernel.toHandle(), kernel2.toHandle()};
    const uint32_t numKernels = 2U;
    const auto *pCountBuffer = &numKernels;
    ze_external_semaphore_signal_params_ext_t semSignalParams = {};
    ze_external_semaphore_wait_params_ext_t semWaitParams = {};

    uint64_t memA[16] = {};
    uint64_t memB[16] = {};
    const void *memRange = memA;
    size_t rangeSize = 4;
    ze_copy_region_t copyRegion = {};
    copyRegion.width = 1;
    copyRegion.height = 1;
    copyRegion.depth = 1;
    ze_image_region_t imgRegion = {};
    imgRegion.width = 1;
    imgRegion.height = 1;
    imgRegion.depth = 1;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    L0::Graph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(immCmdListHandle, memA, memB, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(immCmdListHandle, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEventsWithParameters(immCmdListHandle, nullptr, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWriteGlobalTimestamp(immCmdListHandle, memA, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryRangesBarrier(immCmdListHandle, 1, &rangeSize, &memRange, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFill(immCmdListHandle, memA, memB, 4, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyRegion(immCmdListHandle, memA, &copyRegion, 16, 16, memB, &copyRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyFromContext(immCmdListHandle, memA, &otherCtx, memB, 4, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopy(immCmdListHandle, imgA, imgB, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyRegion(immCmdListHandle, imgA, imgB, &imgRegion, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(immCmdListHandle, memA, imgA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(immCmdListHandle, imgA, memA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryPrefetch(immCmdListHandle, memA, 4));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemAdvise(immCmdListHandle, device, memA, 4, ZE_MEMORY_ADVICE_BIAS_CACHED));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(immCmdListHandle, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(immCmdListHandle, nullptr, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendEventReset(immCmdListHandle, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendQueryKernelTimestamps(immCmdListHandle, 1, &eventHandle, memA, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalExternalSemaphoreExt(immCmdListHandle, 1, &sem, &semSignalParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitExternalSemaphoreExt(immCmdListHandle, 1, &sem, &semWaitParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemoryExt(immCmdListHandle, memA, imgA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemoryExt(immCmdListHandle, imgA, memA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(immCmdListHandle, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchCooperativeKernel(immCmdListHandle, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelIndirect(immCmdListHandle, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchMultipleKernelsIndirect(immCmdListHandle, numKernels, pKernelHandles, pCountBuffer, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyWithParameters(immCmdListHandle, memA, memB, sizeof(memA), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFillWithParameters(immCmdListHandle, memA, memB, 4, sizeof(memA), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendHostFunction(immCmdListHandle, nullptr, nullptr, nullptr, nullptr, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));

    ctx.cmdListsToReturn.push_back(new Mock<CommandList>());
    MockExecutableGraph execGraph;
    EXPECT_TRUE(execGraph.usePatchingPreamble);

    auto *graphHwCommands = ctx.cmdListsToReturn[0];
    EXPECT_EQ(0U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryCopyCalled);
    EXPECT_EQ(0U, graphHwCommands->appendWaitOnEventsCalled);
    EXPECT_EQ(0U, graphHwCommands->appendWriteGlobalTimestampCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryRangesBarrierCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryFillCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryCopyRegionCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryCopyFromContextCalled);
    EXPECT_EQ(0U, graphHwCommands->appendImageCopyCalled);
    EXPECT_EQ(0U, graphHwCommands->appendImageCopyRegionCalled);
    EXPECT_EQ(0U, graphHwCommands->appendImageCopyToMemoryCalled);
    EXPECT_EQ(0U, graphHwCommands->appendImageCopyFromMemoryCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryPrefetchCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemAdviseCalled);
    EXPECT_EQ(0U, graphHwCommands->appendSignalEventCalled);
    EXPECT_EQ(0U, graphHwCommands->appendEventResetCalled);
    EXPECT_EQ(0U, graphHwCommands->appendQueryKernelTimestampsCalled);
    EXPECT_EQ(0U, graphHwCommands->appendSignalExternalSemaphoresCalled);
    EXPECT_EQ(0U, graphHwCommands->appendWaitExternalSemaphoresCalled);
    EXPECT_EQ(0U, graphHwCommands->appendImageCopyToMemoryExtCalled);
    EXPECT_EQ(0U, graphHwCommands->appendImageCopyFromMemoryExtCalled);
    EXPECT_EQ(0U, graphHwCommands->appendLaunchKernelCalled);
    EXPECT_EQ(0U, graphHwCommands->appendLaunchKernelIndirectCalled);
    EXPECT_EQ(0U, graphHwCommands->appendLaunchMultipleKernelsIndirectCalled);
    EXPECT_EQ(0U, graphHwCommands->appendLaunchKernelWithParametersCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryCopyWithParametersCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryFillWithParametersCalled);
    EXPECT_EQ(0U, graphHwCommands->appendHostFunctionCalled);
    execGraph.instantiateFrom(srcGraph);
    EXPECT_EQ(1U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyCalled);
    EXPECT_EQ(2U, graphHwCommands->appendWaitOnEventsCalled); // +1 for zeCommandListAppendWaitOnEventsWithParameters
    EXPECT_EQ(1U, graphHwCommands->appendWriteGlobalTimestampCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryRangesBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryFillCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyRegionCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyFromContextCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyRegionCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyToMemoryCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyFromMemoryCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryPrefetchCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemAdviseCalled);
    EXPECT_EQ(2U, graphHwCommands->appendSignalEventCalled); // + 1 for zeCommandListAppendSignalEventWithParameters
    EXPECT_EQ(1U, graphHwCommands->appendEventResetCalled);
    EXPECT_EQ(1U, graphHwCommands->appendQueryKernelTimestampsCalled);
    EXPECT_EQ(1U, graphHwCommands->appendSignalExternalSemaphoresCalled);
    EXPECT_EQ(1U, graphHwCommands->appendWaitExternalSemaphoresCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyToMemoryExtCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyFromMemoryExtCalled);
    EXPECT_EQ(2U, graphHwCommands->appendLaunchKernelCalled); // +1 for zeCommandListAppendLaunchCooperativeKernel
    EXPECT_EQ(1U, graphHwCommands->appendLaunchKernelIndirectCalled);
    EXPECT_EQ(1U, graphHwCommands->appendLaunchMultipleKernelsIndirectCalled);
    EXPECT_EQ(1U, graphHwCommands->appendLaunchKernelWithParametersCalled); // +1 for zeCommandListAppendLaunchKernelWithArguments
    EXPECT_EQ(1U, graphHwCommands->appendLaunchKernelWithArgumentsCalled);  // +1 for zeCommandListAppendLaunchKernelWithArguments
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyWithParametersCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryFillWithParametersCalled);
    EXPECT_EQ(1U, graphHwCommands->appendHostFunctionCalled);
}

TEST_F(GraphTestInstantiationTest, GivenInvalidAppendKernelArgumentWithParametersWhenRecordingGraphThenReturnErrorCode) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_base_desc_t invalidDesc{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};

    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};
    ze_group_size_t zeroGroupSize = {0, 0, 0};

    L0::Graph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &invalidDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &invalidDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, zeroGroupSize, nullptr, nullptr, nullptr, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenFailingAppendKernelWithParametersWhenRecordingGraphThenSegmentCommandCounterSaved) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_base_desc_t invalidDesc{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES};
    ze_group_count_t groupCount = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));

    ASSERT_EQ(1u, srcGraph.segments.size());
    EXPECT_EQ(1u, srcGraph.segments[0].numCommands);
    EXPECT_EQ(1u, srcGraph.recordedApiCommands.commands.size());

    EXPECT_NE(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &invalidDesc, nullptr, 0, nullptr));

    ASSERT_EQ(1u, srcGraph.segments.size());
    EXPECT_EQ(2u, srcGraph.recordedApiCommands.commands.size());
    EXPECT_EQ(srcGraph.recordedApiCommands.commands.size(), srcGraph.segments[0].numCommands);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenNotMutableGraphWhenCapturingAppendKernelWithGraphNodeDescriptorThenInvalidOperationIsReturnedAndCommandIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_OPERATION_EXP, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &graphNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_OPERATION_EXP, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &graphNodeDesc, nullptr, 0, nullptr));

    ASSERT_EQ(1u, srcGraph.segments.size());
    EXPECT_EQ(1u, srcGraph.segments[0].numCommands);
    EXPECT_EQ(1u, srcGraph.recordedApiCommands.commands.size());
    EXPECT_EQ(nullptr, nodeHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenNotMutableGraphWhenCapturingAppendKernelWithGraphNodeDescriptorChainedAfterOtherDescriptorThenInvalidOperationIsReturned) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_command_list_append_launch_kernel_param_cooperative_desc_t cooperativeDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC};
    cooperativeDesc.pNext = &graphNodeDesc;
    ze_group_count_t groupCount = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, false);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_OPERATION_EXP, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &cooperativeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(0u, srcGraph.recordedApiCommands.commands.size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphInstantiationValidation, GivenNotMutableGraphWhenForkingWithAppendKernelWithGraphNodeDescriptorThenInvalidOperationIsReturnedAndForkIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};

    Graph srcGraph(&ctx, true, false);
    Graph *srcGraphPtr = &srcGraph;
    Graph *childGraph = nullptr;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

    auto ret = L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(childCmdlist, childGraph, nullptr, &childCmdlist, kernel.toHandle(), &groupCount, &graphNodeDesc, nullptr, 1U, &forkEventHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_OPERATION_EXP, ret);
    EXPECT_EQ(nullptr, childGraph);
    EXPECT_FALSE(childCmdlist.isCapturingGraph());
    EXPECT_TRUE(srcGraph.getUnjoinedForks().empty());
    EXPECT_TRUE(srcGraph.getSubgraphs().empty());
    EXPECT_EQ(nullptr, nodeHandle);

    srcGraph.stopCapturing();
}

TEST_F(GraphInstantiationValidation, GivenNotMutableGraphWhenJoiningWithAppendKernelWithGraphNodeDescriptorThenInvalidOperationIsReturnedAndJoinIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    auto joinEventHandle = joinEvent.toHandle();
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};

    Graph srcGraph(&ctx, true, false);
    Graph *srcGraphPtr = &srcGraph;
    Graph *childGraph = nullptr;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, nullptr, &childCmdlist, joinEventHandle, 1U, &forkEventHandle);

    auto ret = L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, kernel.toHandle(), &groupCount, &graphNodeDesc, nullptr, 1U, &joinEventHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_OPERATION_EXP, ret);
    EXPECT_TRUE(srcGraph.getJoinedForks().empty());
    EXPECT_EQ(1u, srcGraph.getCapturedCommands().size());
    EXPECT_EQ(nullptr, nodeHandle);

    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);
    srcGraph.stopCapturing();

    EXPECT_TRUE(srcGraph.getUnjoinedForks().empty());
}

TEST_F(GraphInstantiationValidation, GivenMutableGraphWhenCapturingAppendKernelWithGraphNodeDescriptorInForkedSubgraphThenNodeIsReturnedByRootGraph) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(allMutableGraphNodeFlagsExceptKernelInstruction));
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    auto joinEventHandle = joinEvent.toHandle();
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};

    Graph srcGraph(&ctx, true, true);
    Graph *srcGraphPtr = &srcGraph;
    Graph *childGraph = nullptr;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr);

    auto ret = L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(childCmdlist, childGraph, nullptr, &childCmdlist, kernel.toHandle(), &groupCount, &graphNodeDesc, joinEventHandle, 1U, &forkEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    ASSERT_NE(nullptr, childGraph);
    EXPECT_NE(nullptr, nodeHandle);

    L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle);
    srcGraph.stopCapturing();

    uint32_t nodesCount = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, srcGraph.getNodes(&nodesCount, nullptr));
    ASSERT_EQ(1u, nodesCount);
    ze_graph_node_handle_t rootGraphNodeHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, srcGraph.getNodes(&nodesCount, &rootGraphNodeHandle));
    EXPECT_EQ(nodeHandle, rootGraphNodeHandle);
}

TEST_F(GraphInstantiationValidation, GivenMutableGraphWhenCapturingAppendKernelsWithGraphNodeDescriptorsInRootAndForkedSubgraphThenNodesAreIndexedInCaptureOrder) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(allMutableGraphNodeFlagsExceptKernelInstruction));
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    childCmdlist.device = this->device;
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    auto joinEventHandle = joinEvent.toHandle();
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_group_count_t groupCount = {1, 1, 1};

    ze_graph_node_handle_t firstRootNodeHandle = nullptr;
    ze_graph_node_exp_desc_t firstRootNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    firstRootNodeDesc.phNodeHandle = &firstRootNodeHandle;
    ze_graph_node_handle_t childNodeHandle = nullptr;
    ze_graph_node_exp_desc_t childNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    childNodeDesc.phNodeHandle = &childNodeHandle;
    ze_graph_node_handle_t secondRootNodeHandle = nullptr;
    ze_graph_node_exp_desc_t secondRootNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    secondRootNodeDesc.phNodeHandle = &secondRootNodeHandle;

    Graph srcGraph(&ctx, true, true);
    Graph *srcGraphPtr = &srcGraph;
    Graph *childGraph = nullptr;

    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, kernel.toHandle(), &groupCount, &firstRootNodeDesc, nullptr, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, forkEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(childCmdlist, childGraph, nullptr, &childCmdlist, kernel.toHandle(), &groupCount, &childNodeDesc, joinEventHandle, 1U, &forkEventHandle));
    ASSERT_NE(nullptr, childGraph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 1U, &joinEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, kernel.toHandle(), &groupCount, &secondRootNodeDesc, nullptr, 0U, nullptr));
    srcGraph.stopCapturing();

    const std::vector<ze_graph_node_handle_t> expectedNodeHandles = {firstRootNodeHandle, childNodeHandle, secondRootNodeHandle};
    uint32_t nodesCount = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, srcGraph.getNodes(&nodesCount, nullptr));
    ASSERT_EQ(expectedNodeHandles.size(), nodesCount);
    std::vector<ze_graph_node_handle_t> nodeHandles(nodesCount, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, srcGraph.getNodes(&nodesCount, nodeHandles.data()));
    for (uint32_t nodeIndex = 0U; nodeIndex < nodesCount; ++nodeIndex) {
        EXPECT_EQ(expectedNodeHandles[nodeIndex], nodeHandles[nodeIndex]);
        EXPECT_EQ(nodeIndex, GraphNode::fromHandle(nodeHandles[nodeIndex])->getIndex());
    }
}

TEST_F(GraphInstantiationValidation, GivenFlatCaptureWhenCapturingAppendKernelsWithGraphNodeDescriptorThenGraphNodesAreNotCreated) {
    GraphsCleanupGuard graphCleanup;
    processUsesGraphs.store(true);
    ContextStubMock ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};

    RecordedApiCommands flatCapture;
    Graph *graphCaptureTarget = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(cmdlist, graphCaptureTarget, &flatCapture, &cmdlist, kernel.toHandle(), &groupCount, &graphNodeDesc, nullptr, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(cmdlist, graphCaptureTarget, &flatCapture, &cmdlist, kernel.toHandle(), &groupCount, &graphNodeDesc, nullptr, 0U, nullptr));

    EXPECT_EQ(nullptr, nodeHandle);
    EXPECT_EQ(nullptr, flatCapture.externalStorage.graphNode);
    ASSERT_EQ(2u, flatCapture.commands.size());
    for (const auto &command : flatCapture.commands) {
        const auto &closure = std::get<static_cast<size_t>(CaptureApi::zeCommandListAppendLaunchKernelWithParameters)>(command);
        EXPECT_EQ(nullptr, closure.indirectArgs.capturedLaunchKernelState->graphNodeHandle);
        EXPECT_EQ(nullptr, static_cast<const ze_graph_node_exp_desc_t *>(closure.indirectArgs.capturedLaunchKernelState->clonedPNext)->phNodeHandle);
    }
}

TEST_F(GraphTestInstantiationTest, GivenMutableGraphWhenCapturingAppendKernelWithGraphNodeDescriptorThenCommandIsRecorded) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(allMutableGraphNodeFlagsExceptKernelInstruction));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &graphNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &graphNodeDesc, nullptr, 0, nullptr));

    ASSERT_EQ(2u, srcGraph.recordedApiCommands.commands.size());
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelWithParameters, static_cast<CaptureApi>(srcGraph.recordedApiCommands.commands[0].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelWithArguments, static_cast<CaptureApi>(srcGraph.recordedApiCommands.commands[1].index()));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenCapturedAppendKernelsWithGraphNodeDescriptorsWhenUpdatingGlobalOffsetOfNodesThenCapturedKernelsAreUpdatedInTheSameWay) {
    GraphsCleanupGuard graphCleanup;
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(allMutableGraphNodeFlagsExceptKernelInstruction));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    kernel.setGlobalOffsetExp(1U, 2U, 3U);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t withParametersNodeHandle = nullptr;
    ze_graph_node_exp_desc_t withParametersNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    withParametersNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET;
    withParametersNodeDesc.phNodeHandle = &withParametersNodeHandle;
    ze_graph_node_handle_t withArgumentsNodeHandle = nullptr;
    ze_graph_node_exp_desc_t withArgumentsNodeDesc = withParametersNodeDesc;
    withArgumentsNodeDesc.phNodeHandle = &withArgumentsNodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &withParametersNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &withArgumentsNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));

    ASSERT_NE(nullptr, withParametersNodeHandle);
    ASSERT_NE(nullptr, withArgumentsNodeHandle);
    auto withParametersNode = L0::GraphNode::fromHandle(withParametersNodeHandle);
    auto withArgumentsNode = L0::GraphNode::fromHandle(withArgumentsNodeHandle);

    for (const auto *node : {withParametersNode, withArgumentsNode}) {
        auto capturedGlobalOffset = node->getCurrentGlobalOffset();
        EXPECT_EQ(1U, capturedGlobalOffset[0]);
        EXPECT_EQ(2U, capturedGlobalOffset[1]);
        EXPECT_EQ(3U, capturedGlobalOffset[2]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, withParametersNode->updateGlobalOffset(4U, 5U, 6U));
    EXPECT_EQ(ZE_RESULT_SUCCESS, withArgumentsNode->updateGlobalOffset(7U, 8U, 9U));

    auto withParametersGlobalOffset = withParametersNode->getCurrentGlobalOffset();
    EXPECT_EQ(4U, withParametersGlobalOffset[0]);
    EXPECT_EQ(5U, withParametersGlobalOffset[1]);
    EXPECT_EQ(6U, withParametersGlobalOffset[2]);
    auto withArgumentsGlobalOffset = withArgumentsNode->getCurrentGlobalOffset();
    EXPECT_EQ(7U, withArgumentsGlobalOffset[0]);
    EXPECT_EQ(8U, withArgumentsGlobalOffset[1]);
    EXPECT_EQ(9U, withArgumentsGlobalOffset[2]);

    EXPECT_EQ(1U, kernel.privateState.globalOffsets[0]);
    EXPECT_EQ(2U, kernel.privateState.globalOffsets[1]);
    EXPECT_EQ(3U, kernel.privateState.globalOffsets[2]);
}

TEST_F(GraphTestInstantiationTest, GivenMutableGraphWhenCapturingAppendKernelWithGraphNodeDescriptorWithNullNodeHandlePointerThenInvalidArgumentIsReturned) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(allMutableGraphNodeFlagsExceptKernelInstruction));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.phNodeHandle = nullptr;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &graphNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &graphNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(nullptr, srcGraph.recordedApiCommands.externalStorage.graphNode.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenMutableGraphAndGraphNodeDescriptorWithFlagNotSupportedByDeviceWhenCapturingAppendKernelThenUnsupportedFeatureIsReturnedAndCommandIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_SIZE;
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &graphNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &graphNodeDesc, nullptr, 0, nullptr));

    EXPECT_EQ(0u, srcGraph.recordedApiCommands.commands.size());
    EXPECT_EQ(nullptr, nodeHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenMutableGraphAndDeviceWithoutMutableGraphCapabilitiesWhenCapturingAppendKernelWithZeroGraphNodeFlagsThenUnsupportedFeatureIsReturnedAndCommandIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(0);

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.flags = 0u;
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &graphNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &graphNodeDesc, nullptr, 0, nullptr));

    EXPECT_EQ(0u, srcGraph.recordedApiCommands.commands.size());
    EXPECT_EQ(nullptr, nodeHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenMutableGraphAndDeviceSupportingOnlyKernelInstructionWhenCapturingAppendKernelWithZeroGraphNodeFlagsThenUnsupportedFeatureIsReturnedAndCommandIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_INSTRUCTION));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t nodeHandle = nullptr;
    ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    graphNodeDesc.flags = 0u;
    graphNodeDesc.phNodeHandle = &nodeHandle;
    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &graphNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &graphNodeDesc, nullptr, 0, nullptr));

    EXPECT_EQ(0u, srcGraph.recordedApiCommands.commands.size());
    EXPECT_EQ(nullptr, nodeHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenMutableGraphAndDeviceWithMutableGraphCapabilitiesWhenCapturingAppendKernelWithZeroGraphNodeFlagsThenGraphNodeIsCreated) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(allMutableGraphNodeFlagsExceptKernelInstruction));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t parametersNodeHandle = nullptr;
    ze_graph_node_exp_desc_t parametersNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    parametersNodeDesc.flags = 0u;
    parametersNodeDesc.phNodeHandle = &parametersNodeHandle;

    ze_graph_node_handle_t argumentsNodeHandle = nullptr;
    ze_graph_node_exp_desc_t argumentsNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    argumentsNodeDesc.flags = 0u;
    argumentsNodeDesc.phNodeHandle = &argumentsNodeHandle;

    ze_group_count_t groupCount = {1, 1, 1};
    ze_group_size_t groupSize = {1, 1, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &groupCount, &parametersNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, groupCount, groupSize, nullptr, &argumentsNodeDesc, nullptr, 0, nullptr));

    EXPECT_EQ(2u, srcGraph.recordedApiCommands.commands.size());
    EXPECT_NE(nullptr, parametersNodeHandle);
    EXPECT_NE(nullptr, argumentsNodeHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));
}

TEST_F(GraphTestInstantiationTest, GivenMutableGraphWhenCapturingAppendKernelsWithGraphNodeDescriptorsThenGraphNodesAreCreatedAndStoredInGraph) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT));

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_kernel_handle_t kernelHandle = kernel.toHandle();

    ze_graph_node_handle_t parametersNodeHandle = nullptr;
    ze_graph_node_exp_desc_t parametersNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    parametersNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT;
    parametersNodeDesc.phNodeHandle = &parametersNodeHandle;

    ze_graph_node_handle_t argumentsNodeHandle = nullptr;
    ze_graph_node_exp_desc_t argumentsNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    argumentsNodeDesc.flags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT;
    argumentsNodeDesc.phNodeHandle = &argumentsNodeHandle;

    ze_group_count_t parametersGroupCount = {2, 3, 4};
    ze_group_count_t argumentsGroupCount = {5, 6, 7};
    ze_group_size_t groupSize = {4, 2, 1};

    MockGraph srcGraph(&ctx, true, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));
    EXPECT_TRUE(immCmdList->isCapturingGraph());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithParameters(immCmdListHandle, kernelHandle, &parametersGroupCount, &parametersNodeDesc, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernelWithArguments(immCmdListHandle, kernelHandle, argumentsGroupCount, groupSize, nullptr, &argumentsNodeDesc, nullptr, 0, nullptr));

    EXPECT_EQ(nullptr, srcGraph.recordedApiCommands.externalStorage.graphNode.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));

    uint32_t graphNodesCount = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, srcGraph.getNodes(&graphNodesCount, nullptr));
    ASSERT_EQ(2u, graphNodesCount);

    std::array<ze_graph_node_handle_t, 2> graphNodes = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, srcGraph.getNodes(&graphNodesCount, graphNodes.data()));
    EXPECT_EQ(2u, graphNodesCount);
    EXPECT_EQ(parametersNodeHandle, graphNodes[0]);
    EXPECT_EQ(argumentsNodeHandle, graphNodes[1]);

    ze_graph_node_exp_properties_t nodeProperties = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_PROPERTIES};
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::GraphNode::fromHandle(graphNodes[0])->getProperties(&nodeProperties));
    EXPECT_EQ(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, nodeProperties.type);
    EXPECT_EQ(static_cast<ze_mutable_graph_node_exp_flags_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT), nodeProperties.flags);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::GraphNode::fromHandle(graphNodes[1])->getProperties(&nodeProperties));
    EXPECT_EQ(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, nodeProperties.type);
    EXPECT_EQ(static_cast<ze_mutable_graph_node_exp_flags_t>(ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_KERNEL_ARGUMENT), nodeProperties.flags);

    const auto &capturedCommands = srcGraph.getCapturedCommands();
    ASSERT_EQ(2u, capturedCommands.size());
    auto &parametersClosure = std::get<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>>(capturedCommands[0]);
    auto &argumentsClosure = std::get<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>>(capturedCommands[1]);

    auto parametersNode = static_cast<MockGraphNode *>(L0::GraphNode::fromHandle(graphNodes[0]));
    ASSERT_EQ(&parametersClosure.indirectArgs.groupCounts, parametersNode->getGroupCounts());
    EXPECT_EQ(parametersGroupCount.groupCountX, parametersNode->getGroupCounts()->groupCountX);
    EXPECT_EQ(parametersGroupCount.groupCountY, parametersNode->getGroupCounts()->groupCountY);
    EXPECT_EQ(parametersGroupCount.groupCountZ, parametersNode->getGroupCounts()->groupCountZ);
    EXPECT_EQ(nullptr, parametersNode->getGroupSizes());
    EXPECT_EQ(parametersClosure.indirectArgs.capturedKernel.get(), parametersNode->getKernel());
    EXPECT_EQ(L0::ClosureExternalStorage::invalidKernelArgumentsId, parametersNode->argumentsId);
    EXPECT_EQ(nullptr, parametersNode->externalStorage);

    auto argumentsNode = static_cast<MockGraphNode *>(L0::GraphNode::fromHandle(graphNodes[1]));
    ASSERT_EQ(&argumentsClosure.apiArgs.groupCounts, argumentsNode->getGroupCounts());
    EXPECT_EQ(argumentsGroupCount.groupCountX, argumentsNode->getGroupCounts()->groupCountX);
    EXPECT_EQ(argumentsGroupCount.groupCountY, argumentsNode->getGroupCounts()->groupCountY);
    EXPECT_EQ(argumentsGroupCount.groupCountZ, argumentsNode->getGroupCounts()->groupCountZ);
    ASSERT_EQ(&argumentsClosure.apiArgs.groupSizes, argumentsNode->getGroupSizes());
    EXPECT_EQ(groupSize.groupSizeX, argumentsNode->getGroupSizes()->groupSizeX);
    EXPECT_EQ(groupSize.groupSizeY, argumentsNode->getGroupSizes()->groupSizeY);
    EXPECT_EQ(groupSize.groupSizeZ, argumentsNode->getGroupSizes()->groupSizeZ);
    EXPECT_EQ(argumentsClosure.indirectArgs.capturedKernel.get(), argumentsNode->getKernel());
    EXPECT_NE(L0::ClosureExternalStorage::invalidKernelArgumentsId, argumentsClosure.indirectArgs.argumentsId);
    EXPECT_EQ(argumentsClosure.indirectArgs.argumentsId, argumentsNode->argumentsId);
    EXPECT_EQ(&srcGraph.recordedApiCommands.externalStorage, argumentsNode->externalStorage);
}

TEST(GraphGetNodesTest, GivenNotMutableGraphWhenGettingNodesThenInvalidArgumentIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, false};
    graph.stopCapturing();

    uint32_t graphNodesCount = 0U;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, graph.getNodes(&graphNodesCount, nullptr));
}

TEST(GraphGetNodesTest, GivenMutableGraphStillRecordingWhenGettingNodesThenNotReadyIsReturned) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};

    uint32_t graphNodesCount = 0U;
    EXPECT_EQ(ZE_RESULT_NOT_READY, graph.getNodes(&graphNodesCount, nullptr));
}

TEST(GraphGetNodesTest, GivenCountLessThanNumberOfNodesWhenGettingNodesThenOnlyRequestedNumberOfNodesIsRetrieved) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U));
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U));
    graph.stopCapturing();

    uint32_t graphNodesCount = 1U;
    std::array<ze_graph_node_handle_t, 2> graphNodes = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.getNodes(&graphNodesCount, graphNodes.data()));
    EXPECT_EQ(1u, graphNodesCount);
    EXPECT_EQ((*graph.graphNodes)[0]->toHandle(), graphNodes[0]);
    EXPECT_EQ(nullptr, graphNodes[1]);
}

TEST(GraphGetNodesTest, GivenCountGreaterThanNumberOfNodesWhenGettingNodesThenCountIsUpdatedAndAllNodesAreRetrieved) {
    GraphsCleanupGuard graphCleanup;
    ContextStubMock ctx;
    MockGraph graph{&ctx, true, true};
    graph.graphNodes->push_back(std::make_unique<L0::GraphNode>(ZE_GRAPH_NODE_EXP_TYPE_LAUNCH_KERNEL, 0U));
    graph.stopCapturing();

    uint32_t graphNodesCount = 3U;
    std::array<ze_graph_node_handle_t, 3> graphNodes = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, graph.getNodes(&graphNodesCount, graphNodes.data()));
    EXPECT_EQ(1u, graphNodesCount);
    EXPECT_EQ((*graph.graphNodes)[0]->toHandle(), graphNodes[0]);
    EXPECT_EQ(nullptr, graphNodes[1]);
}

TEST_F(GraphTestInstantiationTest, GivenGraphPatchPreambleDebugFlagWhenInstantiatingGraphThenUseDebugSettingForPatchPreamble) {
    GraphsCleanupGuard graphCleanup;

    debugManager.flags.ForceDisableGraphPatchPreamble.set(1);

    MockExecutableGraph execGraph;
    EXPECT_FALSE(execGraph.usePatchingPreamble);
}

TEST_F(GraphTestInstantiationTest, GivenImplicitSyncMutationDebugFlagWhenCreatingExecutableGraphThenUseDebugSettingForImplicitSyncMutation) {
    GraphsCleanupGuard graphCleanup;

    debugManager.flags.ForceGraphImplicitSyncMutation.set(-1);
    MockExecutableGraph defaultExecGraph;
    EXPECT_TRUE(defaultExecGraph.implicitSyncMutation);

    debugManager.flags.ForceGraphImplicitSyncMutation.set(0);
    MockExecutableGraph disabledExecGraph;
    EXPECT_FALSE(disabledExecGraph.implicitSyncMutation);

    debugManager.flags.ForceGraphImplicitSyncMutation.set(1);
    MockExecutableGraph enabledExecGraph;
    EXPECT_TRUE(enabledExecGraph.implicitSyncMutation);
}

TEST_F(GraphTestInstantiationTest, GivenImplicitSyncMutationDisabledWhenExecutingExecutableGraphThenLastExecutionIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;

    MockExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(nullptr, execGraph.lastExecutionCsr);
}

HWTEST_F(GraphImplicitSyncMutationTest, GivenExecutableGraphNotExecutedWhenWaitingForLastExecutionThenCsrIsNotWaitedFor) {
    GraphsCleanupGuard graphCleanup;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    const auto waitCallsBefore = ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load();

    MockExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.waitForLastExecution());
    EXPECT_EQ(waitCallsBefore, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWTEST_F(GraphImplicitSyncMutationTest, GivenExecutableGraphExecutedTwiceWhenWaitingForLastExecutionThenCsrIsWaitedForTaskCountOfLastExecutionOnlyOnce) {
    GraphsCleanupGuard graphCleanup;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));

    MockExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ultCsr, execGraph.lastExecutionCsr);
    EXPECT_EQ(immCmdList->getTaskCount(false), execGraph.lastExecutionTaskCount);

    const auto waitCallsBefore = ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.waitForLastExecution());
    EXPECT_EQ(waitCallsBefore + 1, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(execGraph.lastExecutionTaskCount, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(nullptr, execGraph.lastExecutionCsr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.waitForLastExecution());
    EXPECT_EQ(waitCallsBefore + 1, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphImplicitSyncMutationTest, GivenOtherSubmissionOnSharedCsrAfterGraphFlushWhenExecutingExecutableGraphThenTaskCountOfExecutionTargetIsRecorded) {
    GraphsCleanupGuard graphCleanup;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));
    constexpr TaskCountType otherSubmissionsCount = 3u;
    ultCsr->onFlushImmediateTask = [ultCsr]() { ultCsr->taskCount += otherSubmissionsCount; };

    ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 1};
    ze_event_pool_handle_t hPool = nullptr;
    auto devHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &poolDesc, 1, &devHandle, &hPool));
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
    ze_event_handle_t waitEventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventDesc, &waitEventHandle));

    MockExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 1, &waitEventHandle));
    ultCsr->onFlushImmediateTask = nullptr;

    EXPECT_EQ(immCmdList->getTaskCount(false), execGraph.lastExecutionTaskCount);
    EXPECT_EQ(execGraph.lastExecutionTaskCount + otherSubmissionsCount, ultCsr->peekTaskCount());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(waitEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(hPool));
}

HWTEST_F(GraphImplicitSyncMutationTest, GivenGpuHangWhenWaitingForLastExecutionThenDeviceLostIsReturnedAndNextWaitWaitsAgain) {
    GraphsCleanupGuard graphCleanup;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));

    MockExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 0, nullptr));

    ultCsr->callBaseWaitForCompletionWithTimeout = false;
    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;
    const auto waitCallsBefore = ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, execGraph.waitForLastExecution());
    EXPECT_EQ(ultCsr, execGraph.lastExecutionCsr);

    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.waitForLastExecution());
    EXPECT_EQ(waitCallsBefore + 2, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(nullptr, execGraph.lastExecutionCsr);
}

HWTEST_F(GraphImplicitSyncMutationTest, GivenWaitNotReadyWhenWaitingForLastExecutionThenNotReadyIsReturnedAndNextWaitWaitsAgain) {
    GraphsCleanupGuard graphCleanup;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(immCmdList->getCsr(false));

    MockExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(immCmdList, nullptr, nullptr, 0, nullptr));

    ultCsr->callBaseWaitForCompletionWithTimeout = false;
    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::notReady;
    const auto waitCallsBefore = ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_NOT_READY, execGraph.waitForLastExecution());
    EXPECT_EQ(ultCsr, execGraph.lastExecutionCsr);

    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.waitForLastExecution());
    EXPECT_EQ(waitCallsBefore + 2, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(nullptr, execGraph.lastExecutionCsr);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphImplicitSyncMutationExternalWaitEventTest, GivenPatchPreambleDisabledAndExternalWaitEventWhenExecutingExecutableGraphAgainThenLastExecutionIsWaitedFor) {
    debugManager.flags.ForceDisableGraphPatchPreamble.set(1);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(inOrderCmdList->getCsr(false));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
    const auto waitCallsBefore = ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(inOrderCmdList.get(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(waitCallsBefore, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    const auto firstExecutionTaskCount = execGraph.lastExecutionTaskCount;

    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(inOrderCmdList.get(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(waitCallsBefore + 1, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
    EXPECT_EQ(firstExecutionTaskCount, ultCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphImplicitSyncMutationExternalWaitEventTest, GivenPatchPreambleEnabledAndExternalWaitEventWhenExecutingExecutableGraphAgainThenLastExecutionIsNotWaitedFor) {
    debugManager.flags.ForceDisableGraphPatchPreamble.set(0);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(inOrderCmdList->getCsr(false));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
    const auto waitCallsBefore = ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load();
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(inOrderCmdList.get(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(inOrderCmdList.get(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(waitCallsBefore, ultCsr->waitForCompletionWithTimeoutTaskCountCalled.load());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphImplicitSyncMutationExternalWaitEventTest, GivenPatchPreambleDisabledAndExternalWaitEventAndGpuHangWhenExecutingExecutableGraphAgainThenDeviceLostIsReturnedAndGraphIsNotSubmitted) {
    debugManager.flags.ForceDisableGraphPatchPreamble.set(1);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(inOrderCmdList->getCsr(false));

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.execute(inOrderCmdList.get(), nullptr, nullptr, 0, nullptr));
    const auto firstExecutionTaskCount = ultCsr->peekTaskCount();

    ultCsr->callBaseWaitForCompletionWithTimeout = false;
    ultCsr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::gpuHang;
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, execGraph.execute(inOrderCmdList.get(), nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(firstExecutionTaskCount, ultCsr->peekTaskCount());
    EXPECT_EQ(ultCsr, execGraph.lastExecutionCsr);
    EXPECT_EQ(firstExecutionTaskCount, execGraph.lastExecutionTaskCount);
}

TEST_F(GraphImplicitSyncMutationTest, GivenFailedExecutionWhenExecutingExecutableGraphThenLastExecutionIsNotRecorded) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> commandList;
    commandList.appendBarrierResult = ZE_RESULT_ERROR_UNKNOWN;

    MockExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, execGraph.execute(&commandList, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(nullptr, execGraph.lastExecutionCsr);
}

TEST_F(GraphTestInstantiationTest, givenEventResetClosureAndEnforcedEventsWhenInstantiatingThenUseCapturedEventHandle) {
    struct MockCommandListRecordingEventReset : Mock<CommandList> {
        ze_result_t appendEventReset(ze_event_handle_t hEvent) override {
            appendEventResetCalled++;
            lastAppendedEventReset = hEvent;
            return ZE_RESULT_SUCCESS;
        }

        ze_event_handle_t lastAppendedEventReset = nullptr;
    };

    Mock<CommandList> sourceCmdList;
    MockCommandListRecordingEventReset executionCmdList;
    Mock<Event> capturedEvent;
    Mock<Event> enforcedSignalEvent;
    ClosureExternalStorage externalStorage;
    ExternalCbEventInfoContainer externalCbEventStorage;

    ClosureInstantiateContext instantiateContext = {&externalCbEventStorage, nullptr};

    Closure<CaptureApi::zeCommandListAppendEventReset>::ApiArgs apiArgs = {
        sourceCmdList.toHandle(),
        capturedEvent.toHandle()};
    Closure<CaptureApi::zeCommandListAppendEventReset> closure(apiArgs, externalStorage);

    EventParams enforcedEvents = {
        enforcedSignalEvent.toHandle(),
        0u,
        nullptr};

    auto result = closure.instantiateTo(&executionCmdList, externalStorage, instantiateContext, enforcedEvents);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, executionCmdList.appendEventResetCalled);
    EXPECT_EQ(capturedEvent.toHandle(), executionCmdList.lastAppendedEventReset);
    EXPECT_NE(enforcedSignalEvent.toHandle(), executionCmdList.lastAppendedEventReset);
}

TEST_F(GraphTestInstantiationTest, givenInOrderCmdListAndRegularCbEventWhenInstantiateToGraphThenDoNotRecordExternalCbEvent) {
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

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    auto event = L0::Event::fromHandle(eventHandle);
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    EXPECT_EQ(0u, externalCbSignalEventContainer.size());
    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest, givenImmediateOnlyCbEventRecordedIntoGraphWhenInstantiateToGraphThenSucceeds) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    // Capture only records the command; the immediate signal-event flag is not validated yet.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    // Instantiation replays the recorded command onto a regular command list. The IMMEDIATE-flagged
    // event must be accepted there because it originates from an immediate command list.
    ExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    auto event = L0::Event::fromHandle(eventHandle);
    srcGraph.reset();
    event->destroy();
}

struct GraphInternalEventFixture : public GraphFixture {
    void setUp() {
        GraphFixture::setUp();

        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        ze_command_queue_desc_t queueDesc = {
            .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
            .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
        inOrderCmdList.reset(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        inOrderCmdList->setOrdinal(0);

        module = std::make_unique<Mock<Module>>(device, nullptr);
        kernel = std::make_unique<Mock<KernelImp>>();
        kernel->module = module.get();
    }

    void tearDown() {
        inOrderCmdList.reset();
        GraphFixture::tearDown();
    }

    ze_result_t appendKernelSignalling(ze_event_handle_t eventHandle) {
        ze_group_count_t groupCount = {1, 1, 1};
        return zeCommandListAppendLaunchKernel(inOrderCmdList->toHandle(), kernel->toHandle(), &groupCount, eventHandle, 0, nullptr);
    }

    ze_result_t appendExternalSignal(ze_event_handle_t eventHandle) {
        ze_event_exp_flags_desc_t eventFlagsDesc = {.stype = ZE_STRUCTURE_TYPE_EVENT_EXP_FLAGS_DESC, .flags = ZE_EVENT_EXP_FLAG_APPEND_GRAPH_EXTERNAL};
        return zeCommandListAppendSignalEventWithParameters(inOrderCmdList->toHandle(), &eventFlagsDesc, eventHandle);
    }

    ze_event_handle_t createAggregatedCounterBasedEvent(uint64_t *counterDeviceAddress) {
        ze_event_counter_based_external_aggregate_storage_desc_t storageDesc = {
            .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_AGGREGATE_STORAGE_DESC,
            .deviceAddress = counterDeviceAddress,
            .incrementValue = 1,
            .completionValue = 3};

        ze_event_counter_based_desc_t eventDesc = {
            .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
            .pNext = &storageDesc,
            .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE};

        ze_event_handle_t hEvent = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &hEvent));
        return hEvent;
    }

    void captureKernelSignalling(ze_graph_handle_t &graphHandle, ze_event_handle_t eventHandle) {
        auto cmdListHandle = inOrderCmdList->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr));
        ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(eventHandle));
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &graphHandle, nullptr));
    }

    void captureExternalSignal(ze_graph_handle_t &graphHandle, ze_event_handle_t eventHandle) {
        auto cmdListHandle = inOrderCmdList->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr));
        ASSERT_EQ(ZE_RESULT_SUCCESS, appendExternalSignal(eventHandle));
        ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(cmdListHandle, &graphHandle, nullptr));
    }

    std::unique_ptr<L0::CommandList> inOrderCmdList;
    std::unique_ptr<Mock<Module>> module;
    std::unique_ptr<Mock<KernelImp>> kernel;
};

using GraphInternalEventTest = Test<GraphInternalEventFixture>;

TEST_F(GraphInternalEventTest, givenSignalOnlyClosuresAndEnforcedExternalWaitWhenInstantiatingThenDoNotRegisterWaits) {
    ze_event_counter_based_desc_t eventDesc = {
        .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
        .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL};
    ze_event_handle_t hExternalEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &hExternalEvent));

    Mock<CommandList> executionCmdList;
    Mock<Event> signalEvent;
    ClosureExternalStorage externalStorage;
    ExternalCbEventInfoContainer eventInfo;
    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr};
    EventParams enforcedEvents{signalEvent.toHandle(), 1u, &hExternalEvent};

    Closure<CaptureApi::zeCommandListAppendSignalEvent> signalClosure({executionCmdList.toHandle(), signalEvent.toHandle()}, externalStorage);
    EXPECT_EQ(ZE_RESULT_SUCCESS, signalClosure.instantiateTo(&executionCmdList, externalStorage, instantiateContext, enforcedEvents));

    Closure<CaptureApi::zeCommandListAppendSignalEventWithParameters> signalWithParametersClosure({executionCmdList.toHandle(), nullptr, signalEvent.toHandle()}, externalStorage);
    EXPECT_EQ(ZE_RESULT_SUCCESS, signalWithParametersClosure.instantiateTo(&executionCmdList, externalStorage, instantiateContext, enforcedEvents));

    EXPECT_EQ(2u, executionCmdList.appendSignalEventCalled);
    EXPECT_TRUE(eventInfo.getCbWaitEventInfos().empty());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hExternalEvent));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphInternalEventTest, givenInstantiateContextWithoutExternalEventContainerWhenRegisteringExternalWaitEventsThenNoCommandIdIsRequested) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    Mock<Event> waitEvent;
    ze_event_handle_t hWaitEvent = waitEvent.toHandle();
    ClosureInstantiateContext instantiateContext{};

    registerExternalCbWaitEvents(1u, &hWaitEvent, instantiateContext, &executionCmdList, true, ExecutableGraphNodeCommand::invalidCommandId);

    EXPECT_EQ(0u, executionCmdList.mutableCommandList.getNextCommandIdCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphInternalEventTest, givenTimestampQueriesWhenGraphIsInstantiatedThenQuerySourcesUseInternalEventsWithoutChangingCapturedHandles) {
    struct QueryRecordingCommandList : CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> {
        using CommandIdRecordingCommandList<FamilyType::gfxCoreFamily>::CommandIdRecordingCommandList;

        ze_result_t appendQueryKernelTimestamps(uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr, const size_t *pOffsets,
                                                ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                                CmdListWaitEventParameters &waitEventsParameters) override {
            this->appendQueryKernelTimestampsCalled++;
            queryEvents.assign(phEvents, phEvents + numEvents);
            waitEvents.clear();
            if (numWaitEvents > 0) {
                waitEvents.assign(phWaitEvents, phWaitEvents + numWaitEvents);
            } else {
                EXPECT_EQ(nullptr, phWaitEvents);
                EXPECT_EQ(0u, this->mutableCommandList.getNextCommandIdCalled);
            }
            return ZE_RESULT_SUCCESS;
        }

        std::vector<ze_event_handle_t> queryEvents;
        std::vector<ze_event_handle_t> waitEvents;
    };

    GraphsCleanupGuard graphCleanup;
    ze_event_counter_based_desc_t eventDesc = {
        .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
        .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP};
    ze_event_handle_t hEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &hEvent));

    eventDesc.flags |= ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;
    ze_event_handle_t hExternalEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &hExternalEvent));

    DriverBackedGraphContextReturningSpecificCmdList ctx(context->getDriverHandle());
    MockGraphCmdListWithContext recordingCmdList{&ctx};
    recordingCmdList.device = this->device;
    auto srcGraph = std::make_unique<MockGraph>(&ctx, true, false);
    auto hGraph = srcGraph->toHandle();
    auto hCmdList = recordingCmdList.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    ze_kernel_timestamp_result_t results[2] = {};
    ze_event_handle_t queryEvents[] = {hExternalEvent, hEvent};
    ze_event_handle_t waitEvents[] = {hEvent, hExternalEvent};

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, hEvent, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendQueryKernelTimestamps(hCmdList, 2, queryEvents, results, nullptr, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendQueryKernelTimestamps(hCmdList, 2, queryEvents, results, nullptr, nullptr, 2, waitEvents));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    ExecutableGraph execGraphs[2];
    for (auto &execGraph : execGraphs) {
        auto *executionCmdList = new QueryRecordingCommandList(this->device);
        ctx.cmdListsToReturn.push_back(executionCmdList);

        ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
        auto hInternalEvent = execGraph.getInternalEvents().getInternal(hEvent);
        ASSERT_NE(hEvent, hInternalEvent);
        EXPECT_EQ(2u, executionCmdList->appendQueryKernelTimestampsCalled);
        EXPECT_EQ((std::vector<ze_event_handle_t>{hExternalEvent, hInternalEvent}), executionCmdList->queryEvents);
        EXPECT_EQ((std::vector<ze_event_handle_t>{hInternalEvent, hExternalEvent}), executionCmdList->waitEvents);

        const auto &waitInfos = execGraph.getExternalCbEventInfoContainer()->getCbWaitEventInfos();
        ASSERT_EQ(1u, waitInfos.size());
        EXPECT_EQ(executionCmdList->waitEvents, waitInfos[0].waitEvents);
        EXPECT_EQ(executionCmdList, waitInfos[0].executor);
        EXPECT_EQ(1u, waitInfos[0].commandId);
        EXPECT_EQ(1u, executionCmdList->mutableCommandList.getNextCommandIdCalled);
        EXPECT_EQ((std::vector<ze_mutable_graph_node_exp_flags_t>{ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS}), executionCmdList->mutableCommandList.recordedFlags);

        const auto &closure = std::get<Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps>>(srcGraph->getCapturedCommands().back());
        ASSERT_EQ(2u, closure.indirectArgs.events.size());
        EXPECT_EQ(hExternalEvent, closure.indirectArgs.events[0]);
        EXPECT_EQ(hEvent, closure.indirectArgs.events[1]);
        auto capturedWaitEvents = getClosureWaitEventsList<CaptureApi::zeCommandListAppendQueryKernelTimestamps>(closure.apiArgs, closure.indirectArgs, srcGraph->getExternalStorage());
        ASSERT_EQ(2u, capturedWaitEvents.size());
        EXPECT_EQ(hEvent, capturedWaitEvents[0]);
        EXPECT_EQ(hExternalEvent, capturedWaitEvents[1]);
    }
    EXPECT_EQ(execGraphs[0].getInternalEvents().getInternal(hEvent), execGraphs[1].getInternalEvents().getInternal(hEvent));

    srcGraph.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hExternalEvent));
}

struct GraphNodeInstantiateFixture : public GraphInternalEventFixture {
    void setUp() {
        GraphInternalEventFixture::setUp();

        ze_event_counter_based_desc_t eventDesc = {
            .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
            .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &hExternalEvent));
        externalStorage.graphNodeCreationEnabled = true;
    }

    void tearDown() {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hExternalEvent));
        GraphInternalEventFixture::tearDown();
    }

    std::unique_ptr<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>> createKernelWithParametersClosure(ze_command_list_handle_t hCommandList, ze_mutable_graph_node_exp_flags_t graphNodeFlags) {
        ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
        graphNodeDesc.flags = graphNodeFlags;
        graphNodeDesc.phNodeHandle = &nodeHandle;
        ze_group_count_t groupCount = {1, 1, 1};
        return std::make_unique<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>>(
            Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::ApiArgs{hCommandList, kernel->toHandle(), &groupCount, &graphNodeDesc, nullptr, 0u, nullptr}, externalStorage);
    }

    std::unique_ptr<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>> createKernelWithArgumentsClosure(ze_command_list_handle_t hCommandList, ze_mutable_graph_node_exp_flags_t graphNodeFlags) {
        ze_graph_node_exp_desc_t graphNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
        graphNodeDesc.flags = graphNodeFlags;
        graphNodeDesc.phNodeHandle = &nodeHandle;
        return std::make_unique<Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>>(
            Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::ApiArgs{hCommandList, kernel->toHandle(), {1, 1, 1}, {1, 1, 1}, nullptr, &graphNodeDesc, nullptr, 0u, nullptr}, externalStorage);
    }

    static constexpr ze_mutable_graph_node_exp_flags_t nodeFlags = ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_GLOBAL_OFFSET;

    ze_event_handle_t hExternalEvent = nullptr;
    ze_graph_node_handle_t nodeHandle = nullptr;
    ClosureExternalStorage externalStorage;
    ExternalCbEventInfoContainer eventInfo;
    ExecutableGraphNodeCommands nodeCommands = ExecutableGraphNodeCommands(1u);
};

using GraphNodeInstantiateTest = Test<GraphNodeInstantiateFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenGraphNodeAndNoExternalWaitEventWhenHandlingGraphNodeCommandThenCommandIdIsObtainedWithNodeFlagsAndStoredForNode) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    auto closure = createKernelWithParametersClosure(executionCmdList.toHandle(), nodeFlags);
    ASSERT_NE(nullptr, nodeHandle);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, &nodeCommands};
    EXPECT_EQ(ZE_RESULT_SUCCESS, handleGraphNodeCommandWithExternalCbWaitEvents(nodeHandle, 0u, nullptr, instantiateContext, &executionCmdList));

    EXPECT_EQ(std::vector<ze_mutable_graph_node_exp_flags_t>{nodeFlags}, executionCmdList.mutableCommandList.recordedFlags);
    const auto &nodeCommand = nodeCommands[GraphNode::fromHandle(nodeHandle)->getIndex()];
    EXPECT_EQ(1u, nodeCommand.commandId);
    EXPECT_EQ(&executionCmdList, nodeCommand.commandList);
    EXPECT_TRUE(eventInfo.getCbWaitEventInfos().empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenGraphNodeAndExternalWaitEventWhenHandlingGraphNodeCommandThenWaitEventsFlagIsAddedAndWaitEventsUseNodeCommandId) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    auto closure = createKernelWithParametersClosure(executionCmdList.toHandle(), nodeFlags);
    ASSERT_NE(nullptr, nodeHandle);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, &nodeCommands};
    EXPECT_EQ(ZE_RESULT_SUCCESS, handleGraphNodeCommandWithExternalCbWaitEvents(nodeHandle, 1u, &hExternalEvent, instantiateContext, &executionCmdList));

    EXPECT_EQ(std::vector<ze_mutable_graph_node_exp_flags_t>{nodeFlags | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS}, executionCmdList.mutableCommandList.recordedFlags);
    const auto &nodeCommand = nodeCommands[GraphNode::fromHandle(nodeHandle)->getIndex()];
    EXPECT_EQ(&executionCmdList, nodeCommand.commandList);
    const auto &waitInfos = eventInfo.getCbWaitEventInfos();
    ASSERT_EQ(1u, waitInfos.size());
    EXPECT_EQ(nodeCommand.commandId, waitInfos[0].commandId);
    EXPECT_EQ(&executionCmdList, waitInfos[0].executor);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenGraphNodeWithZeroFlagsAndExternalWaitEventWhenHandlingGraphNodeCommandThenZeroFlagsArePassedAndWaitEventsUseNodeCommandId) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    auto closure = createKernelWithParametersClosure(executionCmdList.toHandle(), 0u);
    ASSERT_NE(nullptr, nodeHandle);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, &nodeCommands};
    EXPECT_EQ(ZE_RESULT_SUCCESS, handleGraphNodeCommandWithExternalCbWaitEvents(nodeHandle, 1u, &hExternalEvent, instantiateContext, &executionCmdList));

    EXPECT_EQ(std::vector<ze_mutable_graph_node_exp_flags_t>{0u}, executionCmdList.mutableCommandList.recordedFlags);
    const auto &nodeCommand = nodeCommands[GraphNode::fromHandle(nodeHandle)->getIndex()];
    EXPECT_EQ(&executionCmdList, nodeCommand.commandList);
    const auto &waitInfos = eventInfo.getCbWaitEventInfos();
    ASSERT_EQ(1u, waitInfos.size());
    EXPECT_EQ(nodeCommand.commandId, waitInfos[0].commandId);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenGraphNodeCommandIdFailureWhenHandlingGraphNodeCommandThenErrorIsReturnedAndNothingIsStored) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    executionCmdList.mutableCommandList.getNextCommandIdResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    auto closure = createKernelWithParametersClosure(executionCmdList.toHandle(), nodeFlags);
    ASSERT_NE(nullptr, nodeHandle);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, &nodeCommands};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, handleGraphNodeCommandWithExternalCbWaitEvents(nodeHandle, 1u, &hExternalEvent, instantiateContext, &executionCmdList));

    EXPECT_EQ(nullptr, nodeCommands[0].commandList);
    EXPECT_TRUE(eventInfo.getCbWaitEventInfos().empty());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenInstantiateContextWithoutNodeCommandsWhenHandlingGraphNodeCommandThenCommandIdIsRequestedOnlyForWaitEvents) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    auto closure = createKernelWithParametersClosure(executionCmdList.toHandle(), nodeFlags);
    ASSERT_NE(nullptr, nodeHandle);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, handleGraphNodeCommandWithExternalCbWaitEvents(nodeHandle, 1u, &hExternalEvent, instantiateContext, &executionCmdList));

    EXPECT_EQ(std::vector<ze_mutable_graph_node_exp_flags_t>{ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS}, executionCmdList.mutableCommandList.recordedFlags);
    EXPECT_EQ(1u, eventInfo.getCbWaitEventInfos().size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenNoGraphNodeAndExternalWaitEventWhenHandlingGraphNodeCommandThenCommandIdIsRequestedOnlyForWaitEventsAndNoNodeCommandIsStored) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, &nodeCommands};
    EXPECT_EQ(ZE_RESULT_SUCCESS, handleGraphNodeCommandWithExternalCbWaitEvents(nullptr, 1u, &hExternalEvent, instantiateContext, &executionCmdList));

    EXPECT_EQ(std::vector<ze_mutable_graph_node_exp_flags_t>{ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS}, executionCmdList.mutableCommandList.recordedFlags);
    EXPECT_EQ(nullptr, nodeCommands[0].commandList);
    EXPECT_EQ(1u, eventInfo.getCbWaitEventInfos().size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenKernelWithParametersClosureWithGraphNodeAndExternalWaitEventWhenInstantiatingThenNodeCommandIsStoredAndSharedWithWaitEvents) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    auto closure = createKernelWithParametersClosure(executionCmdList.toHandle(), nodeFlags);
    ASSERT_NE(nullptr, nodeHandle);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, &nodeCommands};
    EventParams enforcedEvents{nullptr, 1u, &hExternalEvent};
    EXPECT_EQ(ZE_RESULT_SUCCESS, closure->instantiateTo(&executionCmdList, externalStorage, instantiateContext, enforcedEvents));

    EXPECT_EQ(std::vector<ze_mutable_graph_node_exp_flags_t>{nodeFlags | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS}, executionCmdList.mutableCommandList.recordedFlags);
    const auto &nodeCommand = nodeCommands[GraphNode::fromHandle(nodeHandle)->getIndex()];
    EXPECT_EQ(&executionCmdList, nodeCommand.commandList);
    const auto &waitInfos = eventInfo.getCbWaitEventInfos();
    ASSERT_EQ(1u, waitInfos.size());
    EXPECT_EQ(nodeCommand.commandId, waitInfos[0].commandId);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphNodeInstantiateTest, givenKernelWithArgumentsClosureWithGraphNodeAndExternalWaitEventWhenInstantiatingThenNodeCommandIsStoredAndSharedWithWaitEvents) {
    CommandIdRecordingCommandList<FamilyType::gfxCoreFamily> executionCmdList(this->device);
    auto closure = createKernelWithArgumentsClosure(executionCmdList.toHandle(), nodeFlags);
    ASSERT_NE(nullptr, nodeHandle);

    ClosureInstantiateContext instantiateContext{&eventInfo, nullptr, nullptr, &nodeCommands};
    EventParams enforcedEvents{nullptr, 1u, &hExternalEvent};
    EXPECT_EQ(ZE_RESULT_SUCCESS, closure->instantiateTo(&executionCmdList, externalStorage, instantiateContext, enforcedEvents));

    EXPECT_EQ(std::vector<ze_mutable_graph_node_exp_flags_t>{nodeFlags | ZE_MUTABLE_GRAPH_NODE_EXP_FLAG_WAIT_EVENTS}, executionCmdList.mutableCommandList.recordedFlags);
    const auto &nodeCommand = nodeCommands[GraphNode::fromHandle(nodeHandle)->getIndex()];
    EXPECT_EQ(&executionCmdList, nodeCommand.commandList);
    const auto &waitInfos = eventInfo.getCbWaitEventInfos();
    ASSERT_EQ(1u, waitInfos.size());
    EXPECT_EQ(nodeCommand.commandId, waitInfos[0].commandId);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphInstantiation, GivenMutableGraphWithGraphNodesWhenInstantiatingThenCommandOfEachNodeIsStoredAtNodeIndex) {
    GraphsCleanupGuard graphCleanup;
    debugManager.flags.OverrideMutableGraphCapability.set(static_cast<int32_t>(allMutableGraphNodeFlagsExceptKernelInstruction));
    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.setModule(&module);
    ze_group_count_t groupCount = {1, 1, 1};

    ze_graph_node_handle_t firstNodeHandle = nullptr;
    ze_graph_node_exp_desc_t firstNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    firstNodeDesc.phNodeHandle = &firstNodeHandle;
    ze_graph_node_handle_t secondNodeHandle = nullptr;
    ze_graph_node_exp_desc_t secondNodeDesc = {ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC};
    secondNodeDesc.phNodeHandle = &secondNodeHandle;

    MockGraph srcGraph(&ctx, true, true);
    Graph *srcGraphPtr = &srcGraph;
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.setGraphCaptureTarget(&srcGraph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, kernel.toHandle(), &groupCount, &firstNodeDesc, nullptr, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, nullptr, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::captureCommand<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(cmdlist, srcGraphPtr, nullptr, cmdListHandle, kernel.toHandle(), &groupCount, &secondNodeDesc, nullptr, 0U, nullptr));
    srcGraph.stopCapturing();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();

    auto executionCmdList = new CommandIdRecordingCommandList<FamilyType::gfxCoreFamily>(this->device);
    ctx.cmdListsToReturn.push_back(executionCmdList);

    GraphInstatiateSettings instantiateAsFlat(nullptr, false);
    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(srcGraph, instantiateAsFlat));

    const auto &nodeCommands = *execGraph.getNodeCommands().get();
    ASSERT_EQ(2u, nodeCommands.size());
    const auto &firstNodeCommand = nodeCommands[GraphNode::fromHandle(firstNodeHandle)->getIndex()];
    EXPECT_EQ(1u, firstNodeCommand.commandId);
    EXPECT_EQ(executionCmdList, firstNodeCommand.commandList);
    const auto &secondNodeCommand = nodeCommands[GraphNode::fromHandle(secondNodeHandle)->getIndex()];
    EXPECT_EQ(2u, secondNodeCommand.commandId);
    EXPECT_EQ(executionCmdList, secondNodeCommand.commandList);
}

TEST_F(GraphInternalEventTest, givenCounterBasedEventWhenGraphIsInstantiatedThenInternalEventPreservesTimestampMode) {
    constexpr ze_event_counter_based_flags_t timestampModes[] = {0, ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP, ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP};
    for (auto timestampMode : timestampModes) {
        GraphsCleanupGuard graphCleanup;

        ze_event_counter_based_desc_t eventDesc = {
            .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
            .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | timestampMode};
        ze_event_handle_t hEvent = nullptr;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &hEvent));
        auto *event = L0::Event::fromHandle(hEvent);

        auto srcGraph = std::make_unique<L0::Graph>(context, true, false);
        auto hGraph = srcGraph->toHandle();
        captureKernelSignalling(hGraph, hEvent);

        ExecutableGraph execGraph;
        ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
        auto *internalEvent = execGraph.getInternalEvents().getInternal(event);
        ASSERT_NE(nullptr, internalEvent);
        EXPECT_NE(event, internalEvent);
        EXPECT_EQ(timestampMode != 0, internalEvent->isEventTimestampFlagSet());
        EXPECT_EQ(timestampMode == ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP, internalEvent->hasKernelMappedTsCapability);
        EXPECT_EQ(eventDesc.flags, internalEvent->getCounterBasedFlags());

        srcGraph.reset();
        EXPECT_EQ(ZE_RESULT_SUCCESS, event->destroy());
    }
}

TEST_F(GraphInternalEventTest, givenTimestampedCbEventWhenGraphIsInstantiatedTwiceThenFirstExecutableGraphKeepsItsTimestampNodeUntilDestroyed) {
    GraphsCleanupGuard graphCleanup;
    auto hEvent = createCounterBasedEvent(context, device, false, ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP);
    auto *event = L0::Event::fromHandle(hEvent);

    auto srcGraph = std::make_unique<L0::Graph>(context, true, false);
    auto hGraph = srcGraph->toHandle();
    captureKernelSignalling(hGraph, hEvent);

    auto firstExecGraph = std::make_unique<MockExecutableGraph>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, firstExecGraph->instantiateFrom(*srcGraph));
    auto *internalEvent = srcGraph->getInternalEvents().getInternal(event);
    ASSERT_NE(nullptr, internalEvent);
    ASSERT_EQ(1u, internalEvent->getInOrderExecEventHelper().getTimestampNodesCount());
    auto *firstNode = internalEvent->getInOrderExecEventHelper().getTimestampNode(0);

    ExecutableGraph secondExecGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, secondExecGraph.instantiateFrom(*srcGraph));
    ASSERT_EQ(1u, internalEvent->getInOrderExecEventHelper().getTimestampNodesCount());
    EXPECT_NE(firstNode, internalEvent->getInOrderExecEventHelper().getTimestampNode(0));

    for (auto &cmdList : firstExecGraph->myCommandLists) {
        cmdList->getInOrderExecInfo()->releaseNotUsedTempTimestampNodes(true);
    }
    auto *allocator = device->getInOrderTimestampAllocator();
    auto *nextNode = allocator->getTag();
    EXPECT_NE(firstNode, nextNode);
    nextNode->returnTag();

    firstExecGraph.reset();
    nextNode = allocator->getTag();
    EXPECT_EQ(firstNode, nextNode);
    nextNode->returnTag();

    srcGraph.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->destroy());
}

TEST_F(GraphInternalEventTest, givenTimestampedCbEventSignalledTwiceInGraphWhenInstantiatedThenFirstSignalTimestampNodeIsKeptByExecutableGraph) {
    GraphsCleanupGuard graphCleanup;
    auto hEvent = createCounterBasedEvent(context, device, false, ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP);
    auto *event = L0::Event::fromHandle(hEvent);

    auto srcGraph = std::make_unique<L0::Graph>(context, true, false);
    auto hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    auto *allocator = device->getInOrderTimestampAllocator();
    auto *firstNode = allocator->getTag();
    firstNode->returnTag();

    MockExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
    auto *internalEvent = srcGraph->getInternalEvents().getInternal(event);
    ASSERT_NE(nullptr, internalEvent);
    ASSERT_EQ(1u, internalEvent->getInOrderExecEventHelper().getTimestampNodesCount());
    EXPECT_NE(firstNode, internalEvent->getInOrderExecEventHelper().getTimestampNode(0));

    for (auto &cmdList : execGraph.myCommandLists) {
        cmdList->getInOrderExecInfo()->releaseNotUsedTempTimestampNodes(true);
    }
    auto *nextNode = allocator->getTag();
    EXPECT_NE(firstNode, nextNode);
    nextNode->returnTag();

    srcGraph.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->destroy());
}

TEST_F(GraphInternalEventTest, givenPoolCounterBasedEventWhenClonedThenInternalEventPreservesTimestampMode) {
    constexpr ze_event_pool_flags_t timestampModes[] = {0, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP};
    for (auto timestampMode : timestampModes) {
        ze_event_pool_counter_based_exp_desc_t counterBasedDesc = {
            .stype = ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC,
            .flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE};
        ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, &counterBasedDesc, timestampMode, 1};
        ze_event_pool_handle_t hPool = nullptr;
        auto hDevice = device->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &poolDesc, 1, &hDevice, &hPool));

        ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
        ze_event_handle_t hEvent = nullptr;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventDesc, &hEvent));
        auto *event = L0::Event::fromHandle(hEvent);

        GraphInternalEvents internalEvents;
        ASSERT_EQ(ZE_RESULT_SUCCESS, internalEvents.addInternalEvent(event, context->toHandle()));
        auto *internalEvent = internalEvents.getInternal(event);
        ASSERT_NE(nullptr, internalEvent);
        EXPECT_NE(event, internalEvent);
        EXPECT_EQ(timestampMode != 0, internalEvent->isEventTimestampFlagSet());
        EXPECT_EQ(timestampMode == ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP, internalEvent->hasKernelMappedTsCapability);
        EXPECT_EQ(timestampMode == ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
                  !!(internalEvent->getCounterBasedFlags() & ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP));
        EXPECT_EQ(timestampMode == ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP,
                  !!(internalEvent->getCounterBasedFlags() & ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP));

        EXPECT_EQ(ZE_RESULT_SUCCESS, event->destroy());
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(hPool));
    }
}

TEST_F(GraphInternalEventTest, givenIpcShareableCbEventWhenAddingInternalEventThenOriginalEventIsPreserved) {
    ze_event_counter_based_desc_t eventDesc = {
        .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
        .flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_IPC};
    ze_event_handle_t hEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &hEvent));
    auto *event = L0::Event::fromHandle(hEvent);
    ASSERT_TRUE(event->isCounterBasedExplicitlyEnabled());
    ASSERT_FALSE(event->isIpcImported());
    EXPECT_FALSE(GraphInternalEvents::isInternalEventDependency(event));

    GraphInternalEvents internalEvents;
    EXPECT_EQ(ZE_RESULT_SUCCESS, internalEvents.addInternalEvent(event, context->toHandle()));
    EXPECT_EQ(nullptr, internalEvents.getInternal(event));
    EXPECT_EQ(hEvent, internalEvents.getInternal(hEvent));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hEvent));
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventSignalledInGraphWhenInstantiatedThenHostSynchronizeAndQueryStatusReportGraphInternalEvent) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(eventHandle);
    ASSERT_FALSE(event->getIsSignalledAsGraphInternalEvent());

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();
    captureKernelSignalling(graphHandle, eventHandle);

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    EXPECT_TRUE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->queryStatus(0));

    event->unsetInOrderExecInfo();
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenExternalCbEventSignalledInGraphWhenInstantiatedThenRestrictionIsNotApplied) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, true);
    auto *event = L0::Event::fromHandle(eventHandle);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();
    captureKernelSignalling(graphHandle, eventHandle);

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventSignalledInGraphWhenReSignalledOutsideAfterInstantiateThenRestrictionIsLifted) {
    GraphsCleanupGuard graphCleanup;

    auto eventHandle = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(eventHandle);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();
    captureKernelSignalling(graphHandle, eventHandle);

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));
    ASSERT_TRUE(event->getIsSignalledAsGraphInternalEvent());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(eventHandle));

    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventSignalledInGraphWhenCaptureEndedThenHostSynchronizeAndQueryStatusReportGraphInternalEvent) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(hEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    captureKernelSignalling(hGraph, hEvent);

    EXPECT_EQ(nullptr, event->getRecordedSignalFrom());
    EXPECT_TRUE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(hEvent));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventSignalledInGraphWhenWaitedOnByCommandListAfterCaptureEndedThenGraphInternalEventIsReported) {
    GraphsCleanupGuard graphCleanup;

    auto hInternalEvent = createCounterBasedEvent(context, device, false);
    auto hExternalEvent = createCounterBasedEvent(context, device, true);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hInternalEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hExternalEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeCommandListAppendWaitOnEvents(hCmdList, 1U, &hInternalEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(hCmdList, 1U, &hExternalEvent));

    srcGraph.reset();
    L0::Event::fromHandle(hInternalEvent)->destroy();
    L0::Event::fromHandle(hExternalEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventSignalledInGraphWhenReSignalledOutsideAfterCaptureEndedThenRestrictionIsLifted) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(hEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    captureKernelSignalling(hGraph, hEvent);
    EXPECT_TRUE(event->getIsSignalledAsGraphInternalEvent());

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));

    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->hostSynchronize(0));
    EXPECT_EQ(ZE_RESULT_NOT_READY, zeEventQueryStatus(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(inOrderCmdList->toHandle(), 1U, &hEvent));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventBothSignalledAndWaitedOnInsideGraphWhenInstantiatedThenReplayIsNotRejected) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(hEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, hEvent, 1U, &hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));
    EXPECT_TRUE(event->getIsSignalledAsGraphInternalEvent());

    ExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenAggregatedCbEventSignalledInGraphWhenCaptureEndedThenRestrictionIsNotApplied) {
    GraphsCleanupGuard graphCleanup;

    void *counterAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, sizeof(uint64_t), sizeof(uint64_t), &counterAlloc));

    auto hEvent = createAggregatedCounterBasedEvent(static_cast<uint64_t *>(counterAlloc));
    auto *event = L0::Event::fromHandle(hEvent);
    ASSERT_TRUE(L0::Event::isAggregatedEvent(event));
    const auto incrementValue = event->getInOrderIncrementValue(1);
    const auto applicationAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress();

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();
    captureKernelSignalling(graphHandle, hEvent);

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    EXPECT_EQ(incrementValue, event->getInOrderExecEventHelper().getAggregatedEventUsageCounter());
    EXPECT_EQ(applicationAddress, event->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(hEvent));
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeCommandListAppendWaitOnEvents(inOrderCmdList->toHandle(), 1U, &hEvent));

    srcGraph.reset();
    event->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(counterAlloc));
}

TEST_F(GraphInternalEventTest, givenRegularEventSignalledInGraphWhenInstantiatedThenRestrictionIsNotApplied) {
    GraphsCleanupGuard graphCleanup;

    ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 1};
    ze_event_pool_handle_t hPool = nullptr;
    auto devHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &poolDesc, 1, &devHandle, &hPool));

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
    ze_event_handle_t eventHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventDesc, &eventHandle));
    auto *event = L0::Event::fromHandle(eventHandle);
    ASSERT_FALSE(event->isCounterBasedExplicitlyEnabled());

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();
    captureKernelSignalling(graphHandle, eventHandle);

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeEventQueryStatus(eventHandle));

    srcGraph.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventPoolDestroy(hPool));
}

TEST_F(GraphInternalEventTest, givenCbEventReSignalledOutsideAfterCaptureWhenInstantiatedThenApplicationBindingIsPreserved) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(hEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    captureKernelSignalling(hGraph, hEvent);

    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));
    ASSERT_TRUE(event->getInOrderExecEventHelper().isDataAssigned());
    const auto applicationAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress();
    const auto applicationSignalValue = event->getInOrderExecBaseSignalValue();
    const auto *applicationAllocation = event->getInOrderExecEventHelper().getDeviceCounterAllocation();

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    EXPECT_EQ(applicationAddress, event->getInOrderExecEventHelper().getBaseDeviceAddress());
    EXPECT_EQ(applicationSignalValue, event->getInOrderExecBaseSignalValue());
    EXPECT_EQ(applicationAllocation, event->getInOrderExecEventHelper().getDeviceCounterAllocation());
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
    EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, event->hostSynchronize(0));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventSignalledInsideForkedSubgraphWhenInstantiatedThenApplicationEventIsLeftUnbound) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t queueDesc = {
        .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    std::unique_ptr<L0::CommandList> childCmdList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    childCmdList->setOrdinal(0);

    auto hForkEvent = createCounterBasedEvent(context, device, false);
    auto hSubgraphEvent = createCounterBasedEvent(context, device, false);
    auto *subgraphEvent = L0::Event::fromHandle(hSubgraphEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    auto hChildCmdList = childCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hForkEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hChildCmdList, kernel->toHandle(), &groupCount, hSubgraphEvent, 1U, &hForkEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hSubgraphEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));
    ASSERT_FALSE(srcGraph->getSubgraphs().empty());
    ASSERT_FALSE(subgraphEvent->getInOrderExecEventHelper().isDataAssigned());

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    EXPECT_FALSE(subgraphEvent->getInOrderExecEventHelper().isDataAssigned());

    srcGraph.reset();
    subgraphEvent->destroy();
    L0::Event::fromHandle(hForkEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenInstantiatedGraphWhenExecutableGraphIsDestroyedThenInternalEventsRemainOwnedBySourceGraph) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(hEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    captureKernelSignalling(hGraph, hEvent);

    auto execGraph = std::make_unique<ExecutableGraph>();
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph->instantiateFrom(*(srcGraph.get())));

    auto hClone = execGraph->getInternalEvents().getInternal(hEvent);
    ASSERT_NE(hEvent, hClone);
    auto *clone = L0::Event::fromHandle(hClone);
    EXPECT_TRUE(clone->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_FALSE(event->getInOrderExecEventHelper().isDataAssigned());

    execGraph.reset();
    EXPECT_EQ(hClone, srcGraph->getInternalEvents().getInternal(hEvent));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventsSignalledInRootAndForkedSubgraphWhenCapturedThenInternalEventsAreCreatedInRootGraphBeforeCaptureEnds) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t queueDesc = {
        .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    std::unique_ptr<L0::CommandList> childCmdList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    childCmdList->setOrdinal(0);

    auto hForkEvent = createCounterBasedEvent(context, device, false);
    auto hSubgraphEvent = createCounterBasedEvent(context, device, false);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    const auto &internalEvents = srcGraph->getInternalEvents();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hForkEvent));
    auto hForkClone = internalEvents.getInternal(hForkEvent);
    EXPECT_NE(hForkEvent, hForkClone);

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(childCmdList->toHandle(), kernel->toHandle(), &groupCount, hSubgraphEvent, 1U, &hForkEvent));
    ASSERT_EQ(1u, srcGraph->getSubgraphs().size());
    EXPECT_EQ(&internalEvents, &srcGraph->getSubgraphs()[0]->getInternalEvents());
    auto hSubgraphClone = internalEvents.getInternal(hSubgraphEvent);
    EXPECT_NE(hSubgraphEvent, hSubgraphClone);

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hSubgraphEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
    EXPECT_EQ(hForkClone, execGraph.getInternalEvents().getInternal(hForkEvent));
    EXPECT_EQ(hSubgraphClone, execGraph.getInternalEvents().getInternal(hSubgraphEvent));

    srcGraph.reset();
    L0::Event::fromHandle(hSubgraphEvent)->destroy();
    L0::Event::fromHandle(hForkEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventsDestroyedBeforeCaptureEndWhenGraphIsInstantiatedThenInternalEventsReplaceThem) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t queueDesc = {
        .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER};
    std::unique_ptr<L0::CommandList> childCmdList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    childCmdList->setOrdinal(0);

    auto hForkEvent = createCounterBasedEvent(context, device, false);
    auto hJoinEvent = createCounterBasedEvent(context, device, false);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    const auto &internalEvents = srcGraph->getInternalEvents();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hForkEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(childCmdList->toHandle(), kernel->toHandle(), &groupCount, hJoinEvent, 1U, &hForkEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hJoinEvent));
    auto hForkClone = internalEvents.getInternal(hForkEvent);
    auto hJoinClone = internalEvents.getInternal(hJoinEvent);
    EXPECT_NE(hForkEvent, hForkClone);
    EXPECT_NE(hJoinEvent, hJoinClone);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hForkEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventDestroy(hJoinEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));
    EXPECT_TRUE(srcGraph->validForInstantiation());

    ExecutableGraph execGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*srcGraph));
    EXPECT_EQ(hForkClone, execGraph.getInternalEvents().getInternal(hForkEvent));
    EXPECT_EQ(hJoinClone, execGraph.getInternalEvents().getInternal(hJoinEvent));
}

TEST_F(GraphInternalEventTest,
       givenExternalFlagPassedToSignalEventAndUsedOnlyAsSuchWhenCapturedThenInternalEventIsNotCreatedButPreviousStateRestored) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendExternalSignal(hEvent));
    EXPECT_EQ(hEvent, srcGraph->getInternalEvents().getInternal(hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    ExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));
    auto hExecGraph = execGraph.toHandle();

    auto event = L0::Event::fromHandle(hEvent);
    EXPECT_FALSE(event->getApiRequiredGraphExternalEvent());

    ASSERT_EQ(ZE_RESULT_SUCCESS, ::zeCommandListAppendGraphExt(hCmdList, hExecGraph, nullptr, nullptr, 0, nullptr));
    EXPECT_TRUE(event->getApiRequiredGraphExternalEvent());

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest,
       givenEventUsedAsBothExternalFlagAndInternalSignalEventWhenCapturedThenInternalEventIsCreatedButExternalStateIsSetAtExecution) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEvent(hCmdList, hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendExternalSignal(hEvent));
    EXPECT_NE(hEvent, srcGraph->getInternalEvents().getInternal(hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    ExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));
    auto hExecGraph = execGraph.toHandle();

    auto event = L0::Event::fromHandle(hEvent);
    EXPECT_FALSE(event->getApiRequiredGraphExternalEvent());

    ASSERT_EQ(ZE_RESULT_SUCCESS, ::zeCommandListAppendGraphExt(hCmdList, hExecGraph, nullptr, nullptr, 0, nullptr));
    EXPECT_TRUE(event->getApiRequiredGraphExternalEvent());

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventSignalledOnlyWithExternalSignalWhenCaptureEndedAndGraphIsDestroyedThenEventIsUsableOutsideGraph) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    captureExternalSignal(hGraph, hEvent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(hEvent, 0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(inOrderCmdList->toHandle(), 1U, &hEvent));

    srcGraph.reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(hEvent, 0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(inOrderCmdList->toHandle(), 1U, &hEvent));

    L0::Event::fromHandle(hEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventSignalledOnlyWithExternalSignalWhenCaptureIsOpenThenQueryStatusReportsEventState) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto hCmdList = inOrderCmdList->toHandle();

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendExternalSignal(hEvent));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(hEvent));

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    srcGraph.reset();
    L0::Event::fromHandle(hEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenExternalSignalRecordedInOneGraphWhenAnotherGraphRecordsOnlyRegularSignalThenEventIsUsableOutsideGraph) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);

    std::unique_ptr<L0::Graph> externalSignalGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hExternalSignalGraph = externalSignalGraph->toHandle();
    captureExternalSignal(hExternalSignalGraph, hEvent);

    std::unique_ptr<L0::Graph> regularSignalGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hRegularSignalGraph = regularSignalGraph->toHandle();
    captureKernelSignalling(hRegularSignalGraph, hEvent);

    EXPECT_NE(hEvent, regularSignalGraph->getInternalEvents().getInternal(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(hEvent, 0));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(inOrderCmdList->toHandle(), 1U, &hEvent));

    regularSignalGraph.reset();
    externalSignalGraph.reset();
    L0::Event::fromHandle(hEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenRegularAndExternalSignalRecordedInOneGraphInEitherOrderWhenCaptureEndedThenRegularSignalUsesSubstituteAndEventIsUsableOutsideGraph) {
    GraphsCleanupGuard graphCleanup;

    auto hRegularFirstEvent = createCounterBasedEvent(context, device, false);
    auto hExternalFirstEvent = createCounterBasedEvent(context, device, false);
    auto hCmdList = inOrderCmdList->toHandle();

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hRegularFirstEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendExternalSignal(hRegularFirstEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendExternalSignal(hExternalFirstEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hExternalFirstEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    for (auto hEvent : {hRegularFirstEvent, hExternalFirstEvent}) {
        EXPECT_NE(hEvent, srcGraph->getInternalEvents().getInternal(hEvent));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(hEvent));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventHostSynchronize(hEvent, 0));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(hCmdList, 1U, &hEvent));
    }

    srcGraph.reset();
    L0::Event::fromHandle(hRegularFirstEvent)->destroy();
    L0::Event::fromHandle(hExternalFirstEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenExternalSignalEventNeverSignalledWhenRegularWaitIsCapturedInAnotherGraphThenWaitIsAccepted) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto hCmdList = inOrderCmdList->toHandle();

    std::unique_ptr<L0::Graph> externalSignalGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hExternalSignalGraph = externalSignalGraph->toHandle();
    captureExternalSignal(hExternalSignalGraph, hEvent);

    std::unique_ptr<L0::Graph> waitingGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hWaitingGraph = waitingGraph->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hWaitingGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(hCmdList, 1U, &hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hWaitingGraph, nullptr));

    waitingGraph.reset();
    externalSignalGraph.reset();
    L0::Event::fromHandle(hEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventSignalledOutsideGraphWhenWaitedOnDuringCaptureThenGraphInternalEventReturned) {
    GraphsCleanupGuard graphCleanup;

    auto hInternalEvent = createCounterBasedEvent(context, device, false);
    auto hExternalEvent = createCounterBasedEvent(context, device, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hInternalEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hExternalEvent));
    ASSERT_TRUE(L0::Event::fromHandle(hInternalEvent)->getInOrderExecEventHelper().isDataAssigned());

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hInternalEvent));
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeCommandListAppendWaitOnEvents(hCmdList, 1U, &hInternalEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hExternalEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    srcGraph.reset();
    L0::Event::fromHandle(hInternalEvent)->destroy();
    L0::Event::fromHandle(hExternalEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenNonExternalCbEventSignalledOnlyInEndedGraphWhenWaitedOnInNewCaptureSessionThenGraphInternalEventReturned) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    std::unique_ptr<L0::Graph> firstGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hFirstGraph = firstGraph->toHandle();
    captureKernelSignalling(hFirstGraph, hEvent);
    ASSERT_FALSE(L0::Event::fromHandle(hEvent)->getInOrderExecEventHelper().isDataAssigned());

    std::unique_ptr<L0::Graph> secondGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hSecondGraph = secondGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hSecondGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hSecondGraph, nullptr));

    firstGraph.reset();
    secondGraph.reset();
    L0::Event::fromHandle(hEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventsSignalledInCaptureAndCapturePausedWhenHostSynchronizeCalledThenGraphCaptureUnsupportedReturnedUntilCaptureEnds) {
    GraphsCleanupGuard graphCleanup;

    auto hInternalEvent = createCounterBasedEvent(context, device, false);
    auto hExternalEvent = createCounterBasedEvent(context, device, true);
    auto *internalEvent = L0::Event::fromHandle(hInternalEvent);
    auto *externalEvent = L0::Event::fromHandle(hExternalEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hInternalEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hExternalEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphPauseCaptureExt(hGraph));

    EXPECT_EQ(nullptr, externalEvent->getRecordedSignalFrom());
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED, internalEvent->hostSynchronize(0));
    EXPECT_EQ(ZE_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED, externalEvent->hostSynchronize(0));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphResumeCaptureExt(hGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, externalEvent->hostSynchronize(0));

    srcGraph.reset();
    internalEvent->destroy();
    externalEvent->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventsSignalledInCaptureWhenReSignalledOutsideWhileCaptureIsPausedThenHostSynchronizeIsNotRestricted) {
    GraphsCleanupGuard graphCleanup;

    auto hInternalEvent = createCounterBasedEvent(context, device, false);
    auto hExternalEvent = createCounterBasedEvent(context, device, true);
    auto *internalEvent = L0::Event::fromHandle(hInternalEvent);
    auto *externalEvent = L0::Event::fromHandle(hExternalEvent);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hInternalEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hExternalEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphPauseCaptureExt(hGraph));

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> regularCmdList(CommandList::create(device, NEO::EngineGroupType::compute, ZE_COMMAND_LIST_FLAG_IN_ORDER, returnValue, false));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hInternalEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(regularCmdList->toHandle(), kernel->toHandle(), &groupCount, hExternalEvent, 0, nullptr));

    for (auto *event : {internalEvent, externalEvent}) {
        auto result = event->hostSynchronize(0);
        EXPECT_NE(ZE_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED, result);
        EXPECT_NE(ZE_RESULT_ERROR_GRAPH_INTERNAL_EVENT, result);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeGraphResumeCaptureExt(hGraph));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    srcGraph.reset();
    internalEvent->destroy();
    externalEvent->destroy();
}

TEST_F(GraphInternalEventTest, givenNeverSignalledCbEventWhenWaitedOnDuringCaptureThenCaptureAndInstantiationSucceed) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(hEvent);
    auto hCmdList = inOrderCmdList->toHandle();
    ASSERT_FALSE(event->getInOrderExecEventHelper().isDataAssigned());
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(hCmdList, 1U, &hEvent));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    ExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    srcGraph.reset();
    event->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, GraphInternalEventTest, givenWaitOnUnsignalledCbEventFollowedBySignalOfSameEventWhenInstantiatedTwiceThenSecondInstanceDoesNotWaitOnFirstInstanceCounter) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto hCmdList = inOrderCmdList->toHandle();

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(hCmdList, 1U, &hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    auto hClone = srcGraph->getInternalEvents().getInternal(hEvent);
    ASSERT_NE(hEvent, hClone);
    auto *clone = L0::Event::fromHandle(hClone);

    MockExecutableGraph firstExecGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, firstExecGraph.instantiateFrom(*(srcGraph.get())));
    ASSERT_TRUE(clone->getInOrderExecEventHelper().isDataAssigned());
    auto firstInstanceCounterAddress = clone->getInOrderExecEventHelper().getBaseDeviceAddress();

    MockExecutableGraph secondExecGraph;
    ASSERT_EQ(ZE_RESULT_SUCCESS, secondExecGraph.instantiateFrom(*(srcGraph.get())));
    ASSERT_EQ(1u, secondExecGraph.myCommandLists.size());
    ASSERT_NE(firstInstanceCounterAddress, clone->getInOrderExecEventHelper().getBaseDeviceAddress());

    auto cmdStream = secondExecGraph.myCommandLists[0]->getCmdContainer().getCommandStream();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream->getCpuBase(), cmdStream->getUsed()));

    size_t waitsOnFirstInstanceCounter = 0;
    for (auto &it : findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end())) {
        auto semWait = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semWait->getSemaphoreGraphicsAddress() == firstInstanceCounterAddress) {
            ++waitsOnFirstInstanceCounter;
        }
    }
    EXPECT_EQ(0u, waitsOnFirstInstanceCounter);

    srcGraph.reset();
    L0::Event::fromHandle(hEvent)->destroy();
}

TEST_F(GraphInternalEventTest, givenCbEventSignalledOutsideGraphWhenReSignalledDuringCaptureAndWaitedOnThenCaptureSucceeds) {
    GraphsCleanupGuard graphCleanup;

    auto hEvent = createCounterBasedEvent(context, device, false);
    auto *event = L0::Event::fromHandle(hEvent);
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));
    ASSERT_TRUE(event->getInOrderExecEventHelper().isDataAssigned());

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    ExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphInternalEventTest, givenAggregatedCbEventSignalledOutsideGraphWhenWaitedOnDuringCaptureThenCaptureSucceeds) {
    GraphsCleanupGuard graphCleanup;

    void *counterAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(device->toHandle(), &deviceDesc, sizeof(uint64_t), sizeof(uint64_t), &counterAlloc));

    auto hEvent = createAggregatedCounterBasedEvent(static_cast<uint64_t *>(counterAlloc));
    auto *event = L0::Event::fromHandle(hEvent);
    ASSERT_TRUE(L0::Event::isAggregatedEvent(event));
    ASSERT_TRUE(event->getInOrderExecEventHelper().isDataAssigned());

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    ExecutableGraph execGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, execGraph.instantiateFrom(*(srcGraph.get())));

    srcGraph.reset();
    event->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(counterAlloc));
}

TEST_F(GraphInternalEventTest, givenRegularEventSignalledOutsideGraphWhenWaitedOnDuringCaptureThenCaptureSucceeds) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 1};
    std::unique_ptr<L0::EventPool> eventPool(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &poolDesc, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
    ze_event_handle_t hEvent = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &hEvent));
    auto *event = L0::Event::fromHandle(hEvent);
    ASSERT_FALSE(event->isCounterBasedExplicitlyEnabled());
    ASSERT_EQ(ZE_RESULT_SUCCESS, appendKernelSignalling(hEvent));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t hGraph = srcGraph->toHandle();
    auto hCmdList = inOrderCmdList->toHandle();
    ze_group_count_t groupCount = {1, 1, 1};

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(hCmdList, hGraph, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(hCmdList, kernel->toHandle(), &groupCount, nullptr, 1U, &hEvent));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(hCmdList, &hGraph, nullptr));

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest, givenInOrderCmdListAndExternalCbEventWhenInstantiateToGraphThenRecordExternalCbEvent) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    auto event = L0::Event::fromHandle(eventHandle);
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    // event used twice in the graph, but should be recorded only once
    EXPECT_EQ(1u, externalCbSignalEventContainer.size());
    bool externalCbEventFound = false;
    for (const auto &entry : externalCbSignalEventContainer) {
        EXPECT_EQ(nullptr, entry.executorCommandList);
        if (event == entry.event) {
            externalCbEventFound = true;
            break;
        }
    }
    EXPECT_TRUE(externalCbEventFound);
    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest,
       givenInOrderCmdListAndTimestampExternalCbEventWhenInstantiateToGraphThenRecordExternalCbEvent) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;
    eventDesc.flags |= ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    auto event = L0::Event::fromHandle(eventHandle);
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    // event used twice in the graph, but should be recorded only once
    EXPECT_EQ(1u, externalCbSignalEventContainer.size());
    bool externalCbEventFound = false;
    bool hasSavedTimestampNodes = false;
    const std::vector<NEO::TagNodeBase *> *previousAppendInstantiationNodes = nullptr;
    const NEO::InOrderExecEventHelper *graphInOrderExecEventHelper = nullptr;
    for (const auto &entry : externalCbSignalEventContainer) {
        EXPECT_EQ(nullptr, entry.executorCommandList);
        if (event == entry.event) {
            externalCbEventFound = true;
            hasSavedTimestampNodes = entry.inOrderExecEventHelper.getAdditionalTimestampNodesCount() > 0 ||
                                     entry.inOrderExecEventHelper.getTimestampNodesCount() > 0;
            previousAppendInstantiationNodes = &entry.previousAppendInstantiationNodes;
            graphInOrderExecEventHelper = &entry.inOrderExecEventHelper;
            break;
        }
    }
    EXPECT_TRUE(externalCbEventFound);
    EXPECT_TRUE(hasSavedTimestampNodes);
    // event used twice, check if 1st append nodes were put away
    ASSERT_NE(nullptr, previousAppendInstantiationNodes);
    EXPECT_NE(0u, previousAppendInstantiationNodes->size());
    ASSERT_NE(nullptr, graphInOrderExecEventHelper);

    // use event on immediate - switch TS nodes to immediate command list
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    // execute a graph - check if event is attached to the state of the graph after execution
    execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    auto &eventHelper = event->getInOrderExecEventHelper();

    EXPECT_EQ(graphInOrderExecEventHelper->getTimestampNodesCount(), eventHelper.getTimestampNodesCount());
    EXPECT_EQ(graphInOrderExecEventHelper->getAdditionalTimestampNodesCount(), eventHelper.getAdditionalTimestampNodesCount());

    // if not ts is availble, then consider found, as search is not needed
    bool tsFound = graphInOrderExecEventHelper->getTimestampNodesCount() == 0;
    for (size_t i = 0; i < graphInOrderExecEventHelper->getTimestampNodesCount(); ++i) {
        if (eventHelper.getTimestampNode(i) == graphInOrderExecEventHelper->getTimestampNode(i)) {
            tsFound = true;
        }
    }
    EXPECT_EQ(true, tsFound);

    bool additionalTsFound = graphInOrderExecEventHelper->getAdditionalTimestampNodesCount() == 0;
    for (size_t i = 0; i < graphInOrderExecEventHelper->getAdditionalTimestampNodesCount(); ++i) {
        if (eventHelper.getAdditionalTimestampNode(i) == graphInOrderExecEventHelper->getAdditionalTimestampNode(i)) {
            additionalTsFound = true;
        }
    }
    EXPECT_EQ(true, additionalTsFound);

    // execute a graph again - check if event is attached to the state of the graph after execution
    execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);

    tsFound = graphInOrderExecEventHelper->getTimestampNodesCount() == 0;
    for (size_t i = 0; i < graphInOrderExecEventHelper->getTimestampNodesCount(); ++i) {
        if (eventHelper.getTimestampNode(i) == graphInOrderExecEventHelper->getTimestampNode(i)) {
            tsFound = true;
        }
    }
    EXPECT_EQ(true, tsFound);

    additionalTsFound = graphInOrderExecEventHelper->getAdditionalTimestampNodesCount() == 0;
    for (size_t i = 0; i < graphInOrderExecEventHelper->getAdditionalTimestampNodesCount(); ++i) {
        if (eventHelper.getAdditionalTimestampNode(i) == graphInOrderExecEventHelper->getAdditionalTimestampNode(i)) {
            additionalTsFound = true;
        }
    }
    EXPECT_EQ(true, additionalTsFound);

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest, givenInOrderCmdListAndExternalCbEventWhenExecuteTwoGraphsThenRecordedExternalCbEventAttachedToStateBeforeInstantiationOfDataUnassignedAndGraphStateAfterExecution) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    auto event = L0::Event::fromHandle(eventHandle);

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    const NEO::InOrderExecEventHelper *graphExecHelper = nullptr;
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer) {
        if (event == entry.event) {
            graphExecHelper = &entry.inOrderExecEventHelper;
            break;
        }
    }
    ASSERT_NE(nullptr, graphExecHelper);
    // 1st executable graph, event attached during instantiation
    NEO::InOrderExecEventHelper *currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_FALSE(currentEventInOrderExecHelper->isDataAssigned());

    ExecutableGraph execGraph2;
    execGraph2.instantiateFrom(*(srcGraph.get()));

    const NEO::InOrderExecEventHelper *graphExecHelper2 = nullptr;
    auto &externalCbSignalEventContainer2 = execGraph2.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer2) {
        if (event == entry.event) {
            graphExecHelper2 = &entry.inOrderExecEventHelper;
            break;
        }
    }
    EXPECT_NE(nullptr, graphExecHelper2);
    EXPECT_NE(graphExecHelper, graphExecHelper2);

    // 2nd executable graph, event attached during instantiation
    currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_FALSE(currentEventInOrderExecHelper->isDataAssigned());

    // 1st executable graph execution, event should be attached to 1st graphExecInfo
    returnValue = execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_EQ(currentEventInOrderExecHelper->getBaseDeviceAddress(), graphExecHelper->getBaseDeviceAddress());
    EXPECT_EQ(currentEventInOrderExecHelper->getEventData()->counterValue, graphExecHelper->getEventData()->counterValue);

    // 2nd executable graph execution, event should be attached to 2nd graphExecInfo
    returnValue = execGraph2.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_EQ(currentEventInOrderExecHelper->getBaseDeviceAddress(), graphExecHelper2->getBaseDeviceAddress());
    EXPECT_EQ(currentEventInOrderExecHelper->getEventData()->counterValue, graphExecHelper2->getEventData()->counterValue);

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest,
       givenExternalCreatedCbEventUsedOnImmediateCommandListWhenInstantiatedOnExecutableGraphThenAfterInstantiationAttachedStatePointsToImmediateCommandList) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    auto event = L0::Event::fromHandle(eventHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandle, 0, nullptr));

    auto immediateEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    auto immediateEventDeviceAddress = immediateEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto immediateEventCounterValue = immediateEventInOrderExecHelper->getEventData()->counterValue;

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    NEO::InOrderExecEventHelper *currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_EQ(immediateEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(immediateEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);

    auto res = execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();

    const NEO::InOrderExecEventHelper *graphExecHelper = nullptr;
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer) {
        if (event == entry.event) {
            graphExecHelper = &entry.inOrderExecEventHelper;
            break;
        }
    }
    ASSERT_NE(nullptr, graphExecHelper);
    auto graphEventDeviceAddress = graphExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto graphEventCounterValue = graphExecHelper->getEventData()->counterValue;
    EXPECT_EQ(graphEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(graphEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest,
       givenTimestampExternalCreatedCbEventUsedOnImmediateCommandListWhenInstantiatedOnExecutableGraphThenAfterInstantiationAttachedStatePointsToImmediateCommandList) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;
    eventDesc.flags |= ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    auto event = L0::Event::fromHandle(eventHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandle, 0, nullptr));

    auto immediateEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    auto immediateEventDeviceAddress = immediateEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto immediateEventCounterValue = immediateEventInOrderExecHelper->getEventData()->counterValue;

    std::vector<NEO::TagNodeBase *> immediateTimestampNodes(immediateEventInOrderExecHelper->getTimestampNodesCount());
    for (uint32_t i = 0; i < immediateEventInOrderExecHelper->getTimestampNodesCount(); ++i) {
        immediateTimestampNodes[i] = immediateEventInOrderExecHelper->getTimestampNode(i);
    }
    std::vector<NEO::TagNodeBase *> immediateAdditionalTimestampNodes(immediateEventInOrderExecHelper->getAdditionalTimestampNodesCount());
    for (uint32_t i = 0; i < immediateEventInOrderExecHelper->getAdditionalTimestampNodesCount(); ++i) {
        immediateAdditionalTimestampNodes[i] = immediateEventInOrderExecHelper->getAdditionalTimestampNode(i);
    }

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    NEO::InOrderExecEventHelper *currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_EQ(immediateEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(immediateEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);
    ASSERT_EQ(immediateTimestampNodes.size(), currentEventInOrderExecHelper->getTimestampNodesCount());
    for (uint32_t i = 0; i < immediateTimestampNodes.size(); ++i) {
        EXPECT_EQ(immediateTimestampNodes[i], currentEventInOrderExecHelper->getTimestampNode(i));
    }
    ASSERT_EQ(immediateAdditionalTimestampNodes.size(), currentEventInOrderExecHelper->getAdditionalTimestampNodesCount());
    for (uint32_t i = 0; i < immediateAdditionalTimestampNodes.size(); ++i) {
        EXPECT_EQ(immediateAdditionalTimestampNodes[i], currentEventInOrderExecHelper->getAdditionalTimestampNode(i));
    }

    auto res = execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();

    const NEO::InOrderExecEventHelper *graphExecHelper = nullptr;
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer) {
        if (event == entry.event) {
            graphExecHelper = &entry.inOrderExecEventHelper;
            break;
        }
    }
    ASSERT_NE(nullptr, graphExecHelper);
    auto graphEventDeviceAddress = graphExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto graphEventCounterValue = graphExecHelper->getEventData()->counterValue;
    EXPECT_EQ(graphEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(graphEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);
    ASSERT_EQ(graphExecHelper->getTimestampNodesCount(), currentEventInOrderExecHelper->getTimestampNodesCount());
    for (uint32_t i = 0; i < graphExecHelper->getTimestampNodesCount(); ++i) {
        EXPECT_EQ(graphExecHelper->getTimestampNode(i), currentEventInOrderExecHelper->getTimestampNode(i));
    }
    ASSERT_EQ(graphExecHelper->getAdditionalTimestampNodesCount(), currentEventInOrderExecHelper->getAdditionalTimestampNodesCount());
    for (uint32_t i = 0; i < graphExecHelper->getAdditionalTimestampNodesCount(); ++i) {
        EXPECT_EQ(graphExecHelper->getAdditionalTimestampNode(i), currentEventInOrderExecHelper->getAdditionalTimestampNode(i));
    }

    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest,
       givenCbEventUsedOnGraphAsExternalFlagWhenInstantiatedOnExecutableGraphAsExternalFlagThenAfterInstantiationAttachedStatePointsToPreviousGraph) {
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

    std::unique_ptr<L0::Graph> otherGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t otherGraphHandle = otherGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExt(commandListHandle, otherGraphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(commandListHandle, &eventFlagsDesc, eventHandle));
    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &otherGraphHandle, nullptr));

    ExecutableGraph otherExecGraph;
    auto ret = otherExecGraph.instantiateFrom(*(otherGraph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_FALSE(event->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_FALSE(event->getApiRequiredGraphExternalEvent());

    ret = otherExecGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(event->getInOrderExecEventHelper().isDataAssigned());
    EXPECT_TRUE(event->getApiRequiredGraphExternalEvent());

    auto otherGraphEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    auto otherGraphEventDeviceAddress = otherGraphEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto otherGraphEventCounterValue = otherGraphEventInOrderExecHelper->getEventData()->counterValue;

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExt(commandListHandle, graphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(commandListHandle, &eventFlagsDesc, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    ret = execGraph.instantiateFrom(*(srcGraph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    NEO::InOrderExecEventHelper *currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_EQ(otherGraphEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(otherGraphEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);

    ret = execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();

    const NEO::InOrderExecEventHelper *graphExecHelper = nullptr;
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer) {
        if (event == entry.event) {
            graphExecHelper = &entry.inOrderExecEventHelper;
            break;
        }
    }
    ASSERT_NE(nullptr, graphExecHelper);
    auto graphEventDeviceAddress = graphExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto graphEventCounterValue = graphExecHelper->getEventData()->counterValue;
    EXPECT_EQ(graphEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(graphEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);

    srcGraph.reset();
    otherGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest,
       givenCbEventUsedOnImmediateCommandListWhenInstantiatedOnExecutableGraphAsExternalFlagThenAfterInstantiationAttachedStatePointsToImmediateCommandList) {
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

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandle, 0, nullptr));

    auto immediateEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    auto immediateEventDeviceAddress = immediateEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto immediateEventCounterValue = immediateEventInOrderExecHelper->getEventData()->counterValue;

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExt(commandListHandle, graphHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendSignalEventWithParameters(commandListHandle, &eventFlagsDesc, eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    auto ret = execGraph.instantiateFrom(*(srcGraph.get()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    NEO::InOrderExecEventHelper *currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();
    EXPECT_EQ(immediateEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(immediateEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);

    ret = execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    currentEventInOrderExecHelper = &event->getInOrderExecEventHelper();

    const NEO::InOrderExecEventHelper *graphExecHelper = nullptr;
    auto &externalCbSignalEventContainer = execGraph.getExternalCbEventInfoContainer()->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer) {
        if (event == entry.event) {
            graphExecHelper = &entry.inOrderExecEventHelper;
            break;
        }
    }
    ASSERT_NE(nullptr, graphExecHelper);
    auto graphEventDeviceAddress = graphExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto graphEventCounterValue = graphExecHelper->getEventData()->counterValue;
    EXPECT_EQ(graphEventDeviceAddress, currentEventInOrderExecHelper->getBaseDeviceAddress() + event->getInOrderAllocationOffset());
    EXPECT_EQ(graphEventCounterValue, currentEventInOrderExecHelper->getEventData()->counterValue);

    srcGraph.reset();
    event->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            WhenForkJoinCbExternalEventsAreUsedThenParentExecutorIsNullChildIsObject) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();
    auto whiteBoxCmdQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(commandList.get())->cmdQImmediate);
    EXPECT_EQ(!FamilyType::isQwordInOrderCounter, whiteBoxCmdQueue->patchPreambleCounter.use32bSemaphore);

    std::unique_ptr<L0::CommandList> subCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    subCommandList->setOrdinal(0);
    auto subCommandListHandle = subCommandList->toHandle();
    auto whiteBoxSubCmdQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(subCommandList.get())->cmdQImmediate);

    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    ze_event_handle_t forkEventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &forkEventHandle));
    auto forkEvent = L0::Event::fromHandle(forkEventHandle);

    ze_event_handle_t joinEventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &joinEventHandle));
    auto joinEvent = L0::Event::fromHandle(joinEventHandle);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, forkEventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(subCommandListHandle, joinEventHandle, 1U, &forkEventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 1U, &joinEventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*srcGraph.get());
    auto execGraphHandle = execGraph.toHandle();

    auto cbEventContainer = execGraph.getExternalCbEventInfoContainer().get();

    bool forkEventFound = false;
    bool joinEventFound = false;

    auto &externalCbSignalEventContainer = cbEventContainer->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer) {
        if (entry.event == forkEvent) {
            EXPECT_EQ(nullptr, entry.executorCommandList);
            forkEventFound = true;
        }
        if (entry.event == joinEvent) {
            EXPECT_NE(nullptr, entry.executorCommandList);
            joinEventFound = true;
        }
    }

    EXPECT_TRUE(forkEventFound);
    EXPECT_TRUE(joinEventFound);

    auto &execContainer = cbEventContainer->getExecutorInfos();
    EXPECT_EQ(2u, execContainer.size());

    bool virtualRootFound = false;
    bool physicalChildFound = false;
    for (const auto &entry : execContainer) {
        if (entry.key == nullptr) {
            virtualRootFound = true;
        } else {
            EXPECT_EQ(subCommandList.get(), entry.key);
            physicalChildFound = true;
        }
        EXPECT_EQ(0u, entry.counter());
        EXPECT_EQ(nullptr, entry.hostAddress());
        EXPECT_EQ(0u, entry.hostGpuAddress());
        EXPECT_EQ(nullptr, entry.hostAllocation());
        EXPECT_EQ(0u, entry.deviceGpuAddress());
        EXPECT_EQ(nullptr, entry.deviceAllocation());
    }

    EXPECT_TRUE(virtualRootFound);
    EXPECT_TRUE(physicalChildFound);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr));

    auto &rootHostPatchPreambleData = cbEventContainer->getPreambleData(nullptr);
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.counter, rootHostPatchPreambleData.counter());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeCpuAddress, rootHostPatchPreambleData.hostAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeGpuAddress, rootHostPatchPreambleData.hostGpuAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeAllocation, rootHostPatchPreambleData.hostAllocation());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, rootHostPatchPreambleData.deviceGpuAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.deviceNodeAllocation, rootHostPatchPreambleData.deviceAllocation());
    auto rootCounter = cbEventContainer->getPreambleCounter(nullptr);
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.counter, rootCounter);
    uint64_t foundCounter = 0x0;
    uint64_t foundDevAddress = 0x0;
    cbEventContainer->getPreambleCounterAndDeviceGpuAddress(nullptr, foundCounter, foundDevAddress);
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.counter, foundCounter);
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, foundDevAddress);

    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.counter, forkEvent->getInOrderExecEventHelper().getPatchPreambleCounter());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeCpuAddress, forkEvent->getInOrderExecEventHelper().getPatchPreambleHostAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeGpuAddress, forkEvent->getInOrderExecEventHelper().getPatchPreambleHostGpuAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeAllocation, forkEvent->getInOrderExecEventHelper().getPatchPreambleHostAllocation());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, forkEvent->getInOrderExecEventHelper().getPatchPreambleDeviceGpuAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.deviceNodeAllocation, forkEvent->getInOrderExecEventHelper().getPatchPreambleDeviceAllocation());

    auto &childHostPatchPreambleData = cbEventContainer->getPreambleData(subCommandList.get());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.counter, childHostPatchPreambleData.counter());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeCpuAddress, childHostPatchPreambleData.hostAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeGpuAddress, childHostPatchPreambleData.hostGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeAllocation, childHostPatchPreambleData.hostAllocation());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, childHostPatchPreambleData.deviceGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeAllocation, childHostPatchPreambleData.deviceAllocation());
    auto childCounter = cbEventContainer->getPreambleCounter(subCommandList.get());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.counter, childCounter);
    foundCounter = 0x0;
    foundDevAddress = 0x0;
    cbEventContainer->getPreambleCounterAndDeviceGpuAddress(subCommandList.get(), foundCounter, foundDevAddress);
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.counter, foundCounter);
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, foundDevAddress);

    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.counter, joinEvent->getInOrderExecEventHelper().getPatchPreambleCounter());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeCpuAddress, joinEvent->getInOrderExecEventHelper().getPatchPreambleHostAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeGpuAddress, joinEvent->getInOrderExecEventHelper().getPatchPreambleHostGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeAllocation, joinEvent->getInOrderExecEventHelper().getPatchPreambleHostAllocation());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, joinEvent->getInOrderExecEventHelper().getPatchPreambleDeviceGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeAllocation, joinEvent->getInOrderExecEventHelper().getPatchPreambleDeviceAllocation());

    auto nonExistingCmdList = reinterpret_cast<L0::CommandList *>(0x123);
    auto &nullPatchPreambleData = cbEventContainer->getPreambleData(nonExistingCmdList);
    EXPECT_EQ(0u, nullPatchPreambleData.counter());
    EXPECT_EQ(nullptr, nullPatchPreambleData.hostAddress());
    EXPECT_EQ(0u, nullPatchPreambleData.hostGpuAddress());
    EXPECT_EQ(nullptr, nullPatchPreambleData.hostAllocation());
    EXPECT_EQ(0u, nullPatchPreambleData.deviceGpuAddress());
    EXPECT_EQ(nullptr, nullPatchPreambleData.deviceAllocation());

    auto nullCounter = cbEventContainer->getPreambleCounter(nonExistingCmdList);
    EXPECT_EQ(0u, nullCounter);

    foundCounter = 0x10;
    foundDevAddress = 0x10;

    cbEventContainer->getPreambleCounterAndDeviceGpuAddress(nonExistingCmdList, foundCounter, foundDevAddress);
    EXPECT_EQ(0u, foundCounter);
    EXPECT_EQ(0u, foundDevAddress);

    srcGraph.reset();
    forkEvent->destroy();
    joinEvent->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            WhenForkAndJoinUsesSingleCbExternalEventsThenOnlySingleAndLastUsedBranchIsUsedForEvent) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    std::unique_ptr<L0::CommandList> subCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    subCommandList->setOrdinal(0);
    auto subCommandListHandle = subCommandList->toHandle();
    auto whiteBoxSubCmdQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(subCommandList.get())->cmdQImmediate);

    ze_event_counter_based_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    ze_event_handle_t eventHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));
    auto event = L0::Event::fromHandle(eventHandle);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandle, 0U, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(subCommandListHandle, eventHandle, 1U, &eventHandle)); // this is last place of signal of the event, so this branch should be used for the event
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 1U, &eventHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*srcGraph.get());
    auto execGraphHandle = execGraph.toHandle();

    EXPECT_TRUE(event->isExternalEvent());
    EXPECT_FALSE(event->isActiveExternalCbEvent());

    auto cbEventContainer = execGraph.getExternalCbEventInfoContainer().get();

    bool eventFound = false;
    auto &externalCbSignalEventContainer = cbEventContainer->getCbSignalEventInfos();
    for (const auto &entry : externalCbSignalEventContainer) {
        if (entry.event == event) {
            EXPECT_NE(nullptr, entry.executorCommandList);
            eventFound = true;
        }
    }
    EXPECT_TRUE(eventFound);

    auto &execContainer = cbEventContainer->getExecutorInfos();
    EXPECT_EQ(1u, execContainer.size());

    bool virtualRootFound = false;
    bool physicalChildFound = false;
    for (const auto &entry : execContainer) {
        if (entry.key == nullptr) {
            virtualRootFound = true;
        } else {
            EXPECT_EQ(subCommandList.get(), entry.key);
            physicalChildFound = true;
        }
        EXPECT_EQ(0u, entry.counter());
        EXPECT_EQ(nullptr, entry.hostAddress());
        EXPECT_EQ(0u, entry.hostGpuAddress());
        EXPECT_EQ(nullptr, entry.hostAllocation());
        EXPECT_EQ(0u, entry.deviceGpuAddress());
        EXPECT_EQ(nullptr, entry.deviceAllocation());
    }

    EXPECT_FALSE(virtualRootFound);
    EXPECT_TRUE(physicalChildFound);

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr));

    auto &rootHostPatchPreambleData = cbEventContainer->getPreambleData(nullptr); // there should be no data for root, as event is attached to child
    EXPECT_EQ(0u, rootHostPatchPreambleData.counter());
    EXPECT_EQ(nullptr, rootHostPatchPreambleData.hostAddress());
    EXPECT_EQ(0u, rootHostPatchPreambleData.hostGpuAddress());
    EXPECT_EQ(nullptr, rootHostPatchPreambleData.hostAllocation());
    EXPECT_EQ(0u, rootHostPatchPreambleData.deviceGpuAddress());
    EXPECT_EQ(nullptr, rootHostPatchPreambleData.deviceAllocation());

    auto &childHostPatchPreambleData = cbEventContainer->getPreambleData(subCommandList.get());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.counter, childHostPatchPreambleData.counter());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeCpuAddress, childHostPatchPreambleData.hostAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeGpuAddress, childHostPatchPreambleData.hostGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeAllocation, childHostPatchPreambleData.hostAllocation());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, childHostPatchPreambleData.deviceGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeAllocation, childHostPatchPreambleData.deviceAllocation());

    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.counter, event->getInOrderExecEventHelper().getPatchPreambleCounter());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeCpuAddress, event->getInOrderExecEventHelper().getPatchPreambleHostAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeGpuAddress, event->getInOrderExecEventHelper().getPatchPreambleHostGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.hostNodeAllocation, event->getInOrderExecEventHelper().getPatchPreambleHostAllocation());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, event->getInOrderExecEventHelper().getPatchPreambleDeviceGpuAddress());
    EXPECT_EQ(whiteBoxSubCmdQueue->patchPreambleCounter.deviceNodeAllocation, event->getInOrderExecEventHelper().getPatchPreambleDeviceAllocation());

    srcGraph.reset();
    event->destroy();
}

template <typename FamilyType>
void GraphExternalWaitEventFixtureInit::testExternalWaitEventRootChild(bool externalWaitRoot, bool externalWaitChild) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    alignas(4) uint8_t semWaitCmdRootMemory[sizeof(MI_SEMAPHORE_WAIT)] = {};
    alignas(4) uint8_t semWaitCmdChildMemory[sizeof(MI_SEMAPHORE_WAIT)] = {};
    uint32_t *semWaitCmdRootDw = reinterpret_cast<uint32_t *>(semWaitCmdRootMemory);
    uint32_t *semWaitCmdChildDw = reinterpret_cast<uint32_t *>(semWaitCmdChildMemory);
    MI_SEMAPHORE_WAIT *semWaitCmdRoot = reinterpret_cast<MI_SEMAPHORE_WAIT *>(semWaitCmdRootMemory);
    MI_SEMAPHORE_WAIT *semWaitCmdChild = reinterpret_cast<MI_SEMAPHORE_WAIT *>(semWaitCmdChildMemory);

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();
    auto whiteBoxRootCmdList = CommandList::whiteboxCast(commandList.get());
    auto rootStream = whiteBoxRootCmdList->getCmdContainer().getCommandStream();

    std::unique_ptr<L0::CommandList> subCommandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    subCommandList->setOrdinal(0);
    auto subCommandListHandle = subCommandList->toHandle();
    auto whiteBoxSubCmdList = CommandList::whiteboxCast(subCommandList.get());
    auto subStream = whiteBoxSubCmdList->getCmdContainer().getCommandStream();

    ze_event_pool_desc_t poolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 4};
    ze_event_pool_handle_t hPool = nullptr;
    auto devHandle = device->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(context->toHandle(), &poolDesc, 1, &devHandle, &hPool));
    auto eventPool = L0::EventPool::fromHandle(hPool);
    ze_event_desc_t eventNonCbDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};

    ze_event_counter_based_desc_t eventCbDesc = {ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    eventCbDesc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL;

    ze_event_handle_t eventHandleRoot = nullptr;
    if (externalWaitRoot) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventCbDesc, &eventHandleRoot));
    } else {
        eventNonCbDesc.index = 0;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventNonCbDesc, &eventHandleRoot));
    }
    auto eventRoot = L0::Event::fromHandle(eventHandleRoot);

    ze_event_handle_t eventHandleChild = nullptr;
    if (externalWaitChild) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventCounterBasedCreate(context->toHandle(), device->toHandle(), &eventCbDesc, &eventHandleChild));
    } else {
        eventNonCbDesc.index = 1;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventNonCbDesc, &eventHandleChild));
    }
    auto eventChild = L0::Event::fromHandle(eventHandleChild);

    // attach cb events for sake of having them correct in order exec info before recording starts
    if (externalWaitRoot) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandleRoot, 0U, nullptr));
    }
    if (externalWaitChild) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandleChild, 0U, nullptr));
    }

    uint64_t eventRootGpuAddress = 0;
    uint64_t eventChildGpuAddress = 0;
    if (externalWaitRoot) {
        eventRootGpuAddress = eventRoot->getInOrderExecEventHelper().getBaseDeviceAddress() + eventRoot->getInOrderAllocationOffset();
    }
    if (externalWaitChild) {
        eventChildGpuAddress = eventChild->getInOrderExecEventHelper().getBaseDeviceAddress() + eventChild->getInOrderAllocationOffset();
    }

    ze_event_handle_t eventHandleFork = nullptr;
    eventNonCbDesc.index = 2;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventNonCbDesc, &eventHandleFork));
    auto eventFork = L0::Event::fromHandle(eventHandleFork);

    ze_event_handle_t eventHandleJoin = nullptr;
    eventNonCbDesc.index = 3;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(hPool, &eventNonCbDesc, &eventHandleJoin));
    auto eventJoin = L0::Event::fromHandle(eventHandleJoin);

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true, false);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    // signal forkEvent to have something for the fork, eventHandleRoot is used as a testing wait event
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandleFork, 1U, &eventHandleRoot));
    // have two wait events - one is testing external cb event, the other is regular event for the fork in sub command list
    ze_event_handle_t forkWaitEvents[] = {eventHandleFork, eventHandleChild};
    // signal jointEvent to have something for the join into a root
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(subCommandListHandle, eventHandleJoin, 2U, forkWaitEvents));
    // join event is waited on root for correctness of the test, but it is not used for the external cb event testing
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, nullptr, 1U, &eventHandleJoin));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    bool multiEngineGraph = srcGraph->isMultiEngineGraph();

    const auto &rootDesc = srcGraph->getCaptureTargetDesc();
    const auto &childs = srcGraph->getSubgraphs();
    ASSERT_NE(0u, childs.size());
    auto childGraph = childs[0];
    const auto &childDesc = childGraph->getCaptureTargetDesc();

    if (externalWaitRoot) {
        EXPECT_EQ(reinterpret_cast<const void *>(&rootDesc.mutableExtDesc), rootDesc.desc.pNext);
        EXPECT_TRUE(srcGraph->isMutableCommandList());
    } else {
        if (multiEngineGraph == false && externalWaitChild) {
            EXPECT_EQ(reinterpret_cast<const void *>(&rootDesc.mutableExtDesc), rootDesc.desc.pNext);
            EXPECT_TRUE(srcGraph->isMutableCommandList());
        } else {
            EXPECT_EQ(nullptr, rootDesc.desc.pNext);
            EXPECT_FALSE(srcGraph->isMutableCommandList());
        }
    }

    if (externalWaitChild) {
        EXPECT_EQ(reinterpret_cast<const void *>(&childDesc.mutableExtDesc), childDesc.desc.pNext);
        EXPECT_TRUE(childGraph->isMutableCommandList());
    } else {
        EXPECT_EQ(nullptr, childDesc.desc.pNext);
        EXPECT_FALSE(childGraph->isMutableCommandList());
    }

    GraphInstatiateSettings settings{nullptr, multiEngineGraph};
    MockExecutableGraph execGraph;
    execGraph.instantiateFrom(*srcGraph.get(), settings);
    auto execGraphHandle = execGraph.toHandle();

    MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *rootCmdList = nullptr;
    MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *childCmdList = nullptr;

    if (externalWaitRoot || (externalWaitChild && multiEngineGraph == false)) {
        rootCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(execGraph.myCommandLists[0].get());
    }
    if (multiEngineGraph && externalWaitChild) {
        childCmdList = static_cast<MutableCommandListCoreFamily<FamilyType::gfxCoreFamily> *>(static_cast<MockExecutableGraph *>(execGraph.subGraphs[0].get())->myCommandLists[0].get());
    }

    L0::MCL::MutableSemaphoreWait *rootSemWait = nullptr;
    L0::MCL::MutableSemaphoreWait *childSemWait = nullptr;
    uint64_t semWaitGpuAddressForRootEvent = 0;
    uint64_t semWaitGpuAddressForChildEvent = 0;

    // when external wait event is used, command list is mutable, stores mutable sem_wait object for wait events
    // and sem wait command gpu address of the command buffer, so the sem wait command can be reprogrammed using async mutable patch list
    // under patch preamble, these gpu addreses can be used to find patching SDI commands
    constexpr size_t semWaitForPatchPreambleIndex = 1;
    constexpr size_t semWaitForForkWaitIndex = 1;
    if (externalWaitRoot) {
        // external wait event was used on immediate, so no patch preamble available - it should be nooped in command view
        ASSERT_NE(nullptr, rootCmdList);
        constexpr size_t rootNoopIndex = 0;
        ASSERT_LT(rootNoopIndex, rootCmdList->mutableSemaphoreWaitCmds.size());
        auto noopPatchPreambleSemWait = rootCmdList->mutableSemaphoreWaitCmds[rootNoopIndex].get();
        ASSERT_NE(nullptr, noopPatchPreambleSemWait);
        EXPECT_EQ(0, memcmp(semWaitNoopSpace, noopPatchPreambleSemWait->getCommandView(), sizeof(MI_SEMAPHORE_WAIT)));

        ASSERT_LT(semWaitForPatchPreambleIndex, rootCmdList->mutableSemaphoreWaitCmds.size());
        rootSemWait = rootCmdList->mutableSemaphoreWaitCmds[semWaitForPatchPreambleIndex].get();
        ASSERT_NE(nullptr, rootSemWait);
        semWaitGpuAddressForRootEvent = rootSemWait->getGpuDestinationAddress();
    }
    if (externalWaitChild) {
        if (multiEngineGraph) {
            // external wait event was used on immediate, so no patch preamble available - it should be nooped in command view
            ASSERT_NE(nullptr, childCmdList);
            constexpr size_t childNoopIndex = semWaitForForkWaitIndex;
            ASSERT_LT(childNoopIndex, childCmdList->mutableSemaphoreWaitCmds.size());
            auto noopPatchPreambleSemWait = childCmdList->mutableSemaphoreWaitCmds[childNoopIndex].get();
            ASSERT_NE(nullptr, noopPatchPreambleSemWait);
            EXPECT_EQ(0, memcmp(semWaitNoopSpace, noopPatchPreambleSemWait->getCommandView(), sizeof(MI_SEMAPHORE_WAIT)));

            constexpr size_t childSemWaitIndex = semWaitForPatchPreambleIndex + semWaitForForkWaitIndex;
            ASSERT_LT(childSemWaitIndex, childCmdList->mutableSemaphoreWaitCmds.size());
            childSemWait = childCmdList->mutableSemaphoreWaitCmds[childSemWaitIndex].get();
        } else {
            // external wait event was used on immediate, so no patch preamble available - it should be nooped in command view
            size_t rootIndices = (externalWaitRoot ? 1 + semWaitForPatchPreambleIndex : 0);
            ASSERT_NE(nullptr, rootCmdList);
            size_t childNoopIndex = semWaitForForkWaitIndex + rootIndices;
            ASSERT_LT(childNoopIndex, rootCmdList->mutableSemaphoreWaitCmds.size());
            auto noopPatchPreambleSemWait = rootCmdList->mutableSemaphoreWaitCmds[childNoopIndex].get();
            ASSERT_NE(nullptr, noopPatchPreambleSemWait);
            EXPECT_EQ(0, memcmp(semWaitNoopSpace, noopPatchPreambleSemWait->getCommandView(), sizeof(MI_SEMAPHORE_WAIT)));

            size_t childSemWaitIndex = semWaitForPatchPreambleIndex + semWaitForForkWaitIndex + rootIndices;
            ASSERT_LT(childSemWaitIndex, rootCmdList->mutableSemaphoreWaitCmds.size());
            childSemWait = rootCmdList->mutableSemaphoreWaitCmds[childSemWaitIndex].get();
        }
        ASSERT_NE(nullptr, childSemWait);
        semWaitGpuAddressForChildEvent = childSemWait->getGpuDestinationAddress();
    }

    // verify append with external wait events stores information to refresh(mutate) data of wait events
    auto externalCbEvents = execGraph.getExternalCbEventInfoContainer().get();
    EXPECT_TRUE(externalCbEvents->externalCbWaitEventsPresent());
    const auto &waitEventsContainer = externalCbEvents->getCbWaitEventInfos();

    size_t expectedWaitInfoSize = 0;
    expectedWaitInfoSize += externalWaitRoot ? 1 : 0;
    expectedWaitInfoSize += externalWaitChild ? 1 : 0;
    ASSERT_EQ(expectedWaitInfoSize, waitEventsContainer.size());
    size_t index = 0;
    uint64_t expectedCmdId = index + 1;
    if (externalWaitRoot) {
        EXPECT_EQ(expectedCmdId, waitEventsContainer[index].commandId);
        EXPECT_EQ(1u, waitEventsContainer[index].waitEvents.size());
        EXPECT_EQ(eventHandleRoot, waitEventsContainer[index].waitEvents[0]);
        ++index;
        if (multiEngineGraph == false) {
            ++expectedCmdId;
        }
    }
    if (externalWaitChild) {
        EXPECT_EQ(expectedCmdId, waitEventsContainer[index].commandId);
        EXPECT_EQ(2u, waitEventsContainer[index].waitEvents.size());
        EXPECT_EQ(eventHandleFork, waitEventsContainer[index].waitEvents[0]);
        EXPECT_EQ(eventHandleChild, waitEventsContainer[index].waitEvents[1]);
    }

    auto sizeBeforeRoot = rootStream->getUsed();
    auto sizeBeforeChild = subStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr));
    auto sizeAfterRoot = rootStream->getUsed();
    auto sizeAfterChild = subStream->getUsed();

    // parse and find patch preamble sdi commands they actually repatch graph cmdlist command buffers having external cb events
    GenCmdList cmdListRoot;
    GenCmdList cmdListChild;
    auto parseSemWaitFromSdi = [](GenCmdList &cmdListToParse, uint64_t semWaitGpuAddress, uint32_t *semWaitCmdDw) {
        size_t semWaitCmdSize = sizeof(MI_SEMAPHORE_WAIT);
        auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdListToParse.begin(), cmdListToParse.end());
        size_t semWaitParsedSize = 0;
        for (uint32_t i = 0; i < sdiCmds.size(); ++i) {
            auto sdiCmd = genCmdCast<MI_STORE_DATA_IMM *>(*sdiCmds[i]);
            if (semWaitGpuAddress == sdiCmd->getAddress()) {
                *semWaitCmdDw = sdiCmd->getDataDword0();
                semWaitCmdDw++;
                semWaitParsedSize += sizeof(uint32_t);
                semWaitGpuAddress += sizeof(uint32_t);
                if (sdiCmd->getStoreQword()) {
                    *semWaitCmdDw = sdiCmd->getDataDword1();
                    semWaitCmdDw++;
                    semWaitParsedSize += sizeof(uint32_t);
                    semWaitGpuAddress += sizeof(uint32_t);
                }
                if (semWaitParsedSize >= semWaitCmdSize) {
                    break;
                }
            }
        }
    };
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdListRoot,
        ptrOffset(rootStream->getCpuBase(), sizeBeforeRoot),
        sizeAfterRoot - sizeBeforeRoot));

    // verify patched sem wait commands point to correct in order exec info gpu addresses
    if (externalWaitRoot) {
        parseSemWaitFromSdi(cmdListRoot, semWaitGpuAddressForRootEvent, semWaitCmdRootDw);
        EXPECT_EQ(eventRootGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdRoot));
    }
    if (multiEngineGraph == false && externalWaitChild) {
        parseSemWaitFromSdi(cmdListRoot, semWaitGpuAddressForChildEvent, semWaitCmdChildDw);
        EXPECT_EQ(eventChildGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdChild));
    }

    if (multiEngineGraph) {
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdListChild,
            ptrOffset(subStream->getCpuBase(), sizeBeforeChild),
            sizeAfterChild - sizeBeforeChild));
        if (externalWaitChild) {
            parseSemWaitFromSdi(cmdListChild, semWaitGpuAddressForChildEvent, semWaitCmdChildDw);
            EXPECT_EQ(eventChildGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdChild));
        }
    }

    // reattach cb events to different command list
    if (externalWaitRoot) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandleRoot, 0U, nullptr));
        eventRootGpuAddress = eventRoot->getInOrderExecEventHelper().getBaseDeviceAddress() + eventRoot->getInOrderAllocationOffset();
    }
    if (externalWaitChild) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(commandListHandle, eventHandleChild, 0U, nullptr));
        eventChildGpuAddress = eventChild->getInOrderExecEventHelper().getBaseDeviceAddress() + eventChild->getInOrderAllocationOffset();
    }

    sizeBeforeRoot = rootStream->getUsed();
    sizeBeforeChild = subStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr));
    sizeAfterRoot = rootStream->getUsed();
    sizeAfterChild = subStream->getUsed();

    cmdListRoot.clear();
    cmdListChild.clear();

    memset(semWaitCmdRootMemory, 0, sizeof(MI_SEMAPHORE_WAIT));
    memset(semWaitCmdChildMemory, 0, sizeof(MI_SEMAPHORE_WAIT));

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdListRoot,
        ptrOffset(rootStream->getCpuBase(), sizeBeforeRoot),
        sizeAfterRoot - sizeBeforeRoot));

    // verify patched sem wait commands point to correct in order exec info gpu addresses
    if (externalWaitRoot) {
        parseSemWaitFromSdi(cmdListRoot, semWaitGpuAddressForRootEvent, semWaitCmdRootDw);
        EXPECT_EQ(eventRootGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdRoot));
    }
    if (multiEngineGraph == false && externalWaitChild) {
        parseSemWaitFromSdi(cmdListRoot, semWaitGpuAddressForChildEvent, semWaitCmdChildDw);
        EXPECT_EQ(eventChildGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdChild));
    }

    if (multiEngineGraph) {
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdListChild,
            ptrOffset(subStream->getCpuBase(), sizeBeforeChild),
            sizeAfterChild - sizeBeforeChild));
        if (externalWaitChild) {
            parseSemWaitFromSdi(cmdListChild, semWaitGpuAddressForChildEvent, semWaitCmdChildDw);
            EXPECT_EQ(eventChildGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdChild));
        }
    }

    srcGraph.reset();
    eventRoot->destroy();
    eventChild->destroy();
    eventFork->destroy();
    eventJoin->destroy();
    eventPool->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationExternalWaitEventNoMultiEngineTest,
            GivenExternalWaitEventsUsedOnRootNotUsedOnChildWhenRecordingGraphThenCommandListDescriptorSet) {
    GraphsCleanupGuard graphCleanup;

    testExternalWaitEventRootChild<FamilyType>(true, false);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationExternalWaitEventNoMultiEngineTest,
            GivenExternalWaitEventsNotUsedOnRootUsedOnChildWhenRecordingGraphThenCommandListDescriptorSet) {
    GraphsCleanupGuard graphCleanup;

    testExternalWaitEventRootChild<FamilyType>(false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationExternalWaitEventNoMultiEngineTest,
            GivenExternalWaitEventsUsedOnRootUsedOnChildWhenRecordingGraphThenCommandListDescriptorSet) {
    GraphsCleanupGuard graphCleanup;

    testExternalWaitEventRootChild<FamilyType>(true, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationExternalWaitEventMultiEngineTest,
            GivenExternalWaitEventsUsedOnRootNotUsedOnChildWhenRecordingGraphThenCommandListDescriptorSet) {
    GraphsCleanupGuard graphCleanup;

    testExternalWaitEventRootChild<FamilyType>(true, false);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationExternalWaitEventMultiEngineTest,
            GivenExternalWaitEventsNotUsedOnRootUsedOnChildWhenRecordingGraphThenCommandListDescriptorSet) {
    GraphsCleanupGuard graphCleanup;

    testExternalWaitEventRootChild<FamilyType>(false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationExternalWaitEventMultiEngineTest,
            GivenExternalWaitEventsUsedOnRootUsedOnChildWhenRecordingGraphThenCommandListDescriptorSet) {
    GraphsCleanupGuard graphCleanup;

    testExternalWaitEventRootChild<FamilyType>(true, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            GraphTestInstantiationTest,
            GivenGraphUsesExternalCbEventsWhenExecutingGraphThenPatchPreambleDispatchesCounterPostSync) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = PIPE_CONTROL::POST_SYNC_OPERATION;

    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();
    auto whiteBoxCmdQueue = static_cast<CommandQueue *>(CommandList::whiteboxCast(commandList.get())->cmdQImmediate);

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

    auto cmdBufferCpuBase = commandList->getCmdContainer().getCommandStream()->getCpuBase();

    auto sizeBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendGraphExp(commandListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr));
    auto sizeAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    auto cbEventContainer = execGraph.getExternalCbEventInfoContainer().get();
    auto &rootHostPatchPreambleData = cbEventContainer->getPreambleData(nullptr);
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.counter, rootHostPatchPreambleData.counter());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.hostNodeGpuAddress, rootHostPatchPreambleData.hostGpuAddress());
    EXPECT_EQ(whiteBoxCmdQueue->patchPreambleCounter.deviceNodeGpuAddress, rootHostPatchPreambleData.deviceGpuAddress());

    auto counter = whiteBoxCmdQueue->patchPreambleCounter.counter;
    auto hostGpuAddress = whiteBoxCmdQueue->patchPreambleCounter.hostNodeGpuAddress;
    auto deviceGpuAddress = whiteBoxCmdQueue->patchPreambleCounter.deviceNodeGpuAddress;

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
            pipeControl->getImmediateData() == counter) {
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

} // namespace ult
} // namespace L0
