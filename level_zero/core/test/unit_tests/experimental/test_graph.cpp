/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/experimental/test_graph.h"

using namespace NEO;

namespace L0 {

namespace ult {

TEST(GraphTestApiCreate, GivenNonNullPNextThenGraphCreateReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    ze_graph_handle_t graph = nullptr;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;
    auto err = ::zeGraphCreateExp(&ctx, &graph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graph);
}

TEST(GraphTestApiCreate, GivenNullContextThenGraphCreateReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ze_graph_handle_t graph = nullptr;
    auto err = ::zeGraphCreateExp(nullptr, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graph);
}

TEST(GraphTestApiCreate, GivenValidContextThenGraphCreateReturnsSuccess) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    ze_graph_handle_t graph = nullptr;
    auto err = ::zeGraphCreateExp(&ctx, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, graph);

    err = ::zeGraphDestroyExp(graph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCreate, GivenInvalidGraphThenGraphDestroyReturnsError) {
    GraphsCleanupGuard graphCleanup;
    auto err = ::zeGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = ::zeCommandListBeginGraphCaptureExp(&cmdlist, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);

    L0::Graph graph(&ctx, true);
    err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNullDestinyGraphThenBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;

    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenValidDestinyGraphThenBeginCaptureReturnsSuccessAndOutputGraphIsTheSameAsInput) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;

    L0::Graph graph(&ctx, true);
    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, &graph);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphEndCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = ::zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureReturnsNewGraph) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;

    auto err = ::zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, retGraph);

    ::zeGraphDestroyExp(retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenCommandListIsNotRecordingThenEndCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    ze_graph_handle_t retGraph = nullptr;
    auto err = ::zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureRequiresOutputGraphPlaceholder) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    auto err = ::zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenDestinyGraphProvidedThenEndCaptureDoesNotRequireOutputGraphPlaceholder) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph graph(&ctx, true);
    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenCommandListIsAlreadyRecordingThenBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;

    L0::Graph graph1(&ctx, true);
    L0::Graph graph2(&ctx, true);
    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph1, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph2, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    err = ::zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, &graph1);
}

TEST(GraphTestApiInstantiate, GivenInvalidSourceGraphThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = ::zeCommandListInstantiateGraphExp(nullptr, &execGraph, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraph);
}

TEST(GraphTestApiInstantiate, GivenInvalidOutputGraphPlaceholderThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    auto err = ::zeCommandListInstantiateGraphExp(&srcGraph, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiInstantiate, GivenNonNullPNextThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = ::zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, &ext);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraph);
}

TEST(GraphTestApiInstantiate, GivenValidSourceGraphThenInstantiateReturnsValidExecutableGraph) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = ::zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, execGraph);

    err = ::zeExecutableGraphDestroyExp(execGraph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiInstantiate, GivenInvalidExecutableGraphThenGraphDestroyReturnsError) {
    GraphsCleanupGuard graphCleanup;
    auto err = ::zeExecutableGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiInstantiate, GivenUnclosedGraphThenInstantiateFails) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);

    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = ::zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, execGraph);
}

TEST(GraphTestDebugApis, GivenNullGraphWhenIsGraphCaptureEnabledIsCalledThenReturnError) {
    GraphsCleanupGuard graphCleanup;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ::zeCommandListIsGraphCaptureEnabledExp(nullptr));
}

TEST(GraphTestDebugApis, GivenCommandListWithoutCaptureEnabledWhenIsGraphCaptureEnabledIsCalledThenReturnFalse) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, ::zeCommandListIsGraphCaptureEnabledExp(&cmdlist));
}

TEST(GraphTestDebugApis, GivenCommandListWithCaptureEnabledWhenIsGraphCaptureEnabledIsCalledThenReturnTrue) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);

    EXPECT_EQ(ZE_RESULT_QUERY_TRUE, ::zeCommandListIsGraphCaptureEnabledExp(&cmdlist));

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, nullptr, 0U, nullptr);

    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, ::zeCommandListIsGraphCaptureEnabledExp(&cmdlist));
}

TEST(GraphTestDebugApis, GivenNullGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ::zeGraphIsEmptyExp(nullptr));
}

TEST(GraphTestDebugApis, GivenInvalidGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<Event> forkEvent;

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &forkEvent, 0U, nullptr);

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, forkEvent);
    srcSubGraph->stopCapturing();
    subCmdlist.setCaptureTarget(nullptr);
    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GRAPH, ::zeGraphIsEmptyExp(&srcGraph));
}

TEST(GraphTestDebugApis, GivenEmptyGraphWhenGraphIsEmptyIsCalledThenTrue) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Graph srcGraph(&ctx, true);

    EXPECT_EQ(ZE_RESULT_QUERY_TRUE, ::zeGraphIsEmptyExp(&srcGraph));
}

TEST(GraphTestDebugApis, GivenNonEmptyGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, nullptr, 0U, nullptr);

    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, ::zeGraphIsEmptyExp(&srcGraph));
}

TEST(GraphTestDebugApis, WhenGraphDumpContentsIsCalledThenReturnUnsupportedFeature) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto err = ::zeGraphDumpContentsExp(&srcGraph, "dump", nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphTestApiSubmit, GivenNonNullPNextThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = ::zeCommandListAppendGraphExp(&cmdlist, &execGraph, &ext, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiSubmit, GivenInvalidCmdListThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);
    auto err = ::zeCommandListAppendGraphExp(nullptr, &execGraph, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiSubmit, GivenInvalidGraphThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;

    auto err = ::zeCommandListAppendGraphExp(&cmdlist, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiSubmit, GivenValidCmdListAndGraphThenGraphAppendReturnsSuccess) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    auto err = ::zeCommandListAppendGraphExp(&cmdlist, &execGraph, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCapture, GivenCommandListInRecordStateThenCaptureCommandsInsteadOfExecutingThem) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<Context> otherCtx;
    Mock<CommandList> cmdlist;
    Mock<Event> event;
    Mock<KernelImp> kernel;
    ze_image_handle_t imgA = nullptr;
    ze_image_handle_t imgB = nullptr;
    ze_device_handle_t device = nullptr;
    ze_kernel_handle_t kernelHandle = &kernel;
    ze_external_semaphore_ext_handle_t sem = nullptr;
    ze_event_handle_t eventHandle = &event;
    ze_external_semaphore_signal_params_ext_t semSignalParams = {};
    ze_external_semaphore_wait_params_ext_t semWaitParams = {};
    uint32_t kernelCount = 1;

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

    L0::Graph graph(&ctx, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendBarrier(&cmdlist, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryCopy(&cmdlist, memA, memB, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendWaitOnEvents(&cmdlist, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendWriteGlobalTimestamp(&cmdlist, memA, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryRangesBarrier(&cmdlist, 1, &rangeSize, &memRange, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryFill(&cmdlist, memA, memB, 4, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryCopyRegion(&cmdlist, memA, &copyRegion, 16, 16, memB, &copyRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryCopyFromContext(&cmdlist, memA, &otherCtx, memB, 4, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopy(&cmdlist, imgA, imgB, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyRegion(&cmdlist, imgA, imgB, &imgRegion, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyToMemory(&cmdlist, memA, imgA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyFromMemory(&cmdlist, imgA, memA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryPrefetch(&cmdlist, memA, 4));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemAdvise(&cmdlist, device, memA, 4, ZE_MEMORY_ADVICE_BIAS_CACHED));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendSignalEvent(&cmdlist, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendEventReset(&cmdlist, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendQueryKernelTimestamps(&cmdlist, 1, &eventHandle, memA, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendSignalExternalSemaphoreExt(&cmdlist, 1, &sem, &semSignalParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendWaitExternalSemaphoreExt(&cmdlist, 1, &sem, &semWaitParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyToMemoryExt(&cmdlist, memA, imgA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyFromMemoryExt(&cmdlist, imgA, memA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendLaunchKernel(&cmdlist, kernelHandle, &groupCount, nullptr, 0, nullptr));

    // temporarily unsupported
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendLaunchCooperativeKernel(&cmdlist, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendLaunchKernelIndirect(&cmdlist, kernelHandle, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendLaunchMultipleKernelsIndirect(&cmdlist, 1, &kernelHandle, &kernelCount, &groupCount, nullptr, 0, nullptr));

    ze_graph_handle_t hgraph = &graph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, ::zeCommandListEndGraphCaptureExp(&cmdlist, &hgraph, nullptr));

    ASSERT_EQ(22U, graph.getCapturedCommands().size());
    uint32_t i = 0;
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendWaitOnEvents, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
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
    EXPECT_EQ(CaptureApi::zeCommandListAppendEventReset, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendQueryKernelTimestamps, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopyToMemoryExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendImageCopyFromMemoryExt, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernel, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
}

TEST(GraphForks, GivenUnknownChildCommandlistThenJoinDoesNothing) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> childCmdlist;
    Mock<Event> joinEvent;
    Graph parent{&ctx, true};
    parent.tryJoinOnNextCommand(childCmdlist, joinEvent);
    EXPECT_TRUE(parent.getSubgraphs().empty());
}

TEST(GraphForks, GivenNullEventThenRecordHandleSignaleEventDoesNothing) {
    GraphsCleanupGuard graphCleanup;
    struct MockGraph : Graph {
        using Graph::Graph;
        using Graph::recordedSignals;
    };
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    MockGraph graph{&ctx, true};
    recordHandleSignalEventFromPreviousCommand(cmdlist, graph, nullptr);
    EXPECT_TRUE(graph.recordedSignals.empty());
}

TEST(GraphForks, GivenRegularEventDependencyThenCommandlistDoesNotStartRecording) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Graph graph{&ctx, true};

    Mock<Event> regularEvent;
    ze_event_handle_t hEvent = &regularEvent;
    Mock<CommandList> cmdlist;
    auto err = L0::zeCommandListAppendWaitOnEvents(&cmdlist, 1, &hEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, cmdlist.getCaptureTarget());
}

TEST(GraphInstantiation, GivenSourceGraphThenExecutableIsInstantiatedProperly) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    Mock<CommandList> subCmdlist;
    Mock<Event> signalEvents[3];
    Mock<Event> waitEvents[3];
    ze_event_handle_t waitEventsList[3] = {&waitEvents[0], &waitEvents[1], &waitEvents[2]};
    ctx.cmdListToReturn = new Mock<CommandList>();
    auto *graphCmdList = ctx.cmdListToReturn;
    uint64_t memA[16] = {};
    uint64_t memB[16] = {};

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &signalEvents[0], 2U, waitEventsList);
    cmdlist.capture<CaptureApi::zeCommandListAppendMemoryCopy>(&cmdlist, memA, memB, sizeof(memA), &signalEvents[1], 3U, waitEventsList);
    srcGraph.stopCapturing();
    EXPECT_FALSE(srcGraph.isSubGraph());

    ASSERT_EQ(2U, srcGraph.getCapturedCommands().size());
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[0].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[1].index()));

    GraphInstatiateSettings instantiateAsMonolithic;
    instantiateAsMonolithic.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);
    EXPECT_FALSE(execGraph.isSubGraph());
    EXPECT_FALSE(execGraph.empty());
    EXPECT_TRUE(execGraph.getSubgraphs().empty());
    EXPECT_EQ(1U, graphCmdList->appendBarrierCalled);
    EXPECT_EQ(1U, graphCmdList->appendMemoryCopyCalled);

    ctx.cmdListToReturn = new Mock<CommandList>();
    graphCmdList = ctx.cmdListToReturn;

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, signalEvents[0]);
    srcGraph.tryJoinOnNextCommand(subCmdlist, signalEvents[2]);

    ASSERT_EQ(1U, srcGraph.getSubgraphs().size());
    EXPECT_TRUE(srcGraph.getSubgraphs()[0]->isSubGraph());
    EXPECT_TRUE(srcGraph.getSubgraphs()[0]->empty());

    ExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);

    EXPECT_FALSE(execMultiGraph.isSubGraph());
    EXPECT_EQ(1U, graphCmdList->appendBarrierCalled);
    EXPECT_EQ(1U, graphCmdList->appendMemoryCopyCalled);
    ASSERT_EQ(1U, execMultiGraph.getSubgraphs().size());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->isSubGraph());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->empty());
}

TEST(GraphInstantiation, GivenSourceGraphWhenPolicyIsSetToInterleaveThenExecutableInterleavesParentAndChildCommandListBasedOnForks) {
    GraphsCleanupGuard graphCleanup;

    struct MockExecutableGraph : ExecutableGraph {
        using ExecutableGraph::ExecutableGraph;
        using ExecutableGraph::submissionChain;
    };

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t hForkEvent = &forkEvent;
    ze_event_handle_t hJoinEvent = &joinEvent;

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &forkEvent, 0U, nullptr);

    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, &joinEvent, 1U, &hForkEvent);

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, nullptr, 1U, &hJoinEvent);

    srcGraph.stopCapturing();

    {
        GraphInstatiateSettings instantiateAsMonolithic;
        instantiateAsMonolithic.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;

        MockExecutableGraph execMultiGraph;
        execMultiGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);
        EXPECT_EQ(2U, execMultiGraph.submissionChain.size()); // parent -> child
    }

    {
        GraphInstatiateSettings instantiateAsInterleaved;
        instantiateAsInterleaved.forkPolicy = GraphInstatiateSettings::ForkPolicySplitLevels;

        MockExecutableGraph execMultiGraph;
        execMultiGraph.instantiateFrom(srcGraph, instantiateAsInterleaved);
        EXPECT_EQ(3U, execMultiGraph.submissionChain.size()); // parent0 -> child -> parent1
    }
}

TEST(GraphInstantiationValidation, WhenGraphIsStillCapturingThenItIsNotValidForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Graph srcGraph(&ctx, true);
    EXPECT_FALSE(srcGraph.validForInstantiation());
    srcGraph.stopCapturing();
    EXPECT_TRUE(srcGraph.validForInstantiation());
}

TEST(GraphInstantiationValidation, WhenGraphHasUnjoinedForksThenItIsNotValidForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    MockGraphCmdListWithContext childCmdlist{&ctx};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    { // missing join
        Graph srcGraph(&ctx, true);
        Graph *srcGraphPtr = &srcGraph;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, &cmdlist, &forkEvent, 0U, nullptr);
        Graph *childGraph = nullptr;
        srcGraph.forkTo(childCmdlist, childGraph, forkEvent);
        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.validForInstantiation());
        childCmdlist.setCaptureTarget(nullptr);
    }

    { // correct graph
        Graph srcGraph(&ctx, true);
        Graph *srcGraphPtr = &srcGraph;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, &cmdlist, &forkEvent, 0U, nullptr);
        Graph *childGraph = nullptr;
        srcGraph.forkTo(childCmdlist, childGraph, forkEvent);
        srcGraph.tryJoinOnNextCommand(childCmdlist, joinEvent);
        srcGraph.stopCapturing();
        EXPECT_TRUE(srcGraph.validForInstantiation());
    }
}

TEST(GraphInstantiationValidation, WhenSubGraphsAreNotValidForInstantiationThenWholeGraphIsNotReadyForInstantiation) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    MockGraphCmdListWithContext childCmdlist{&ctx};
    MockGraphCmdListWithContext grandChildCmdlist{&ctx};
    Mock<Event> forkEvent;
    Mock<Event> forkEventLvl2;
    Mock<Event> joinEvent;
    Mock<Event> joinEventLvl2;
    { // missing join
        Graph srcGraph(&ctx, true);
        Graph *srcGraphPtr = &srcGraph;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, &cmdlist, &forkEvent, 0U, nullptr);
        Graph *childGraph = nullptr;
        srcGraph.forkTo(childCmdlist, childGraph, forkEvent);

        {
            Graph *grandChildGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, &childCmdlist, &forkEventLvl2, 0U, nullptr);
            childGraph->forkTo(grandChildCmdlist, grandChildGraph, forkEventLvl2);
            grandChildCmdlist.setCaptureTarget(nullptr);
        }

        srcGraph.tryJoinOnNextCommand(childCmdlist, joinEvent);
        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.validForInstantiation());
    }

    { // correct graph
        Graph srcGraph(&ctx, true);
        Graph *srcGraphPtr = &srcGraph;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, &cmdlist, &forkEvent, 0U, nullptr);
        Graph *childGraph = nullptr;
        srcGraph.forkTo(childCmdlist, childGraph, forkEvent);

        {
            Graph *grandChildGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, &childCmdlist, &forkEventLvl2, 0U, nullptr);
            childGraph->forkTo(grandChildCmdlist, grandChildGraph, forkEventLvl2);
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildGraph, &grandChildCmdlist, &joinEventLvl2, 0U, nullptr);
            childGraph->tryJoinOnNextCommand(grandChildCmdlist, joinEventLvl2);
        }

        srcGraph.tryJoinOnNextCommand(childCmdlist, joinEvent);
        srcGraph.stopCapturing();
        EXPECT_TRUE(srcGraph.validForInstantiation());
    }
}

TEST(GraphTestInstantiation, WhenInstantiatingGraphThenBakeCommandsIntoCommandlists) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    MockGraphContextReturningSpecificCmdList otherCtx;
    Mock<CommandList> cmdlist;
    Mock<Event> event;
    Mock<KernelImp> kernel;
    ze_image_handle_t imgA = nullptr;
    ze_image_handle_t imgB = nullptr;
    zes_device_handle_t device = nullptr;
    ze_external_semaphore_ext_handle_t sem = nullptr;
    ze_event_handle_t eventHandle = &event;
    zet_kernel_handle_t kernelHandle = &kernel;
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

    L0::Graph srcGraph(&ctx, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &srcGraph, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendBarrier(&cmdlist, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryCopy(&cmdlist, memA, memB, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendWaitOnEvents(&cmdlist, 1, &eventHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendWriteGlobalTimestamp(&cmdlist, memA, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryRangesBarrier(&cmdlist, 1, &rangeSize, &memRange, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryFill(&cmdlist, memA, memB, 4, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryCopyRegion(&cmdlist, memA, &copyRegion, 16, 16, memB, &copyRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryCopyFromContext(&cmdlist, memA, &otherCtx, memB, 4, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopy(&cmdlist, imgA, imgB, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyRegion(&cmdlist, imgA, imgB, &imgRegion, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyToMemory(&cmdlist, memA, imgA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyFromMemory(&cmdlist, imgA, memA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemoryPrefetch(&cmdlist, memA, 4));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendMemAdvise(&cmdlist, device, memA, 4, ZE_MEMORY_ADVICE_BIAS_CACHED));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendSignalEvent(&cmdlist, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendEventReset(&cmdlist, &event));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendQueryKernelTimestamps(&cmdlist, 1, &eventHandle, memA, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendSignalExternalSemaphoreExt(&cmdlist, 1, &sem, &semSignalParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendWaitExternalSemaphoreExt(&cmdlist, 1, &sem, &semWaitParams, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyToMemoryExt(&cmdlist, memA, imgA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendImageCopyFromMemoryExt(&cmdlist, imgA, memA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, L0::zeCommandListAppendLaunchKernel(&cmdlist, kernelHandle, &groupCount, nullptr, 0, nullptr));

    ze_graph_handle_t hgraph = &srcGraph;
    EXPECT_EQ(ZE_RESULT_SUCCESS, ::zeCommandListEndGraphCaptureExp(&cmdlist, &hgraph, nullptr));

    ctx.cmdListToReturn = new Mock<CommandList>();
    ExecutableGraph execGraph;

    auto *graphHwCommands = ctx.cmdListToReturn;
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
    execGraph.instantiateFrom(srcGraph);
    EXPECT_EQ(1U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyCalled);
    EXPECT_EQ(1U, graphHwCommands->appendWaitOnEventsCalled);
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
    EXPECT_EQ(1U, graphHwCommands->appendSignalEventCalled);
    EXPECT_EQ(1U, graphHwCommands->appendEventResetCalled);
    EXPECT_EQ(1U, graphHwCommands->appendQueryKernelTimestampsCalled);
    EXPECT_EQ(1U, graphHwCommands->appendSignalExternalSemaphoresCalled);
    EXPECT_EQ(1U, graphHwCommands->appendWaitExternalSemaphoresCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyToMemoryExtCalled);
    EXPECT_EQ(1U, graphHwCommands->appendImageCopyFromMemoryExtCalled);
    EXPECT_EQ(1U, graphHwCommands->appendLaunchKernelCalled);
}

TEST(GraphExecution, GivenEmptyExecutableGraphWhenSubmittingItToCommandListThenTakeCareOnlyOfEvents) {
    GraphsCleanupGuard graphCleanup;
    Mock<Event> signalEvents[2];
    Mock<Event> waitEvents[3];
    ze_event_handle_t waitEventsList[3] = {&waitEvents[0], &waitEvents[1], &waitEvents[2]};

    Mock<CommandList> cmdlist;
    ExecutableGraph graph;
    EXPECT_TRUE(graph.empty());

    auto res = graph.execute(&cmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, cmdlist.appendSignalEventCalled);

    res = graph.execute(&cmdlist, nullptr, &signalEvents[0], 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(1U, cmdlist.appendSignalEventCalled);

    res = graph.execute(&cmdlist, nullptr, &signalEvents[0], 2, waitEventsList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(1U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(2U, cmdlist.appendSignalEventCalled);

    res = graph.execute(&cmdlist, nullptr, nullptr, 3, waitEventsList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(2U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(2U, cmdlist.appendSignalEventCalled);
}

TEST(GraphExecution, GivenExecutableGraphWhenSubmittingItToCommandListThenAppendIt) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandList> cmdlist;

    ctx.cmdListToReturn = new Mock<CommandList>();

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, nullptr, 0U, nullptr);
    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    auto res = execGraph.execute(&cmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(1U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, cmdlist.appendSignalEventCalled);
}

TEST(GraphExecution, GivenExecutableGraphWithSubGraphsWhenSubmittingItToCommandListSubmitAlsoSubGraphsToRespectiveCommandLists) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t hSignalEventParent = &signalEventParent;
    ze_event_handle_t hSignalEventChild = &signalEventChild;

    Graph srcGraph(&ctx, true);
    mainRecordCmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&mainRecordCmdlist, &signalEventParent, 0U, nullptr);

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, signalEventParent);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&subCmdlist, &signalEventChild, 1U, &hSignalEventParent);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&mainRecordCmdlist, nullptr, 1U, &hSignalEventChild);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setCaptureTarget(nullptr);

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
}

} // namespace ult
} // namespace L0
