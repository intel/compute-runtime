/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/include/level_zero/driver_experimental/zex_graph.h"

using namespace NEO;

namespace L0 {
ze_result_t zeCommandListAppendWriteGlobalTimestamp(ze_command_list_handle_t hCommandList, uint64_t *dstptr, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendBarrier(ze_command_list_handle_t hCommandList, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryRangesBarrier(ze_command_list_handle_t hCommandList, uint32_t numRanges, const size_t *pRangeSizes, const void **pRanges, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryCopy(ze_command_list_handle_t hCommandList, void *dstptr, const void *srcptr, size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryFill(ze_command_list_handle_t hCommandList, void *ptr, const void *pattern, size_t patternSize, size_t size, ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryCopyRegion(ze_command_list_handle_t hCommandList, void *dstptr, const ze_copy_region_t *dstRegion, uint32_t dstPitch, uint32_t dstSlicePitch, const void *srcptr, const ze_copy_region_t *srcRegion, uint32_t srcPitch, uint32_t srcSlicePitch, ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryCopyFromContext(ze_command_list_handle_t hCommandList, void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr, size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopy(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyRegion(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage, const ze_image_region_t *pDstRegion, const ze_image_region_t *pSrcRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyToMemory(ze_command_list_handle_t hCommandList, void *dstptr, ze_image_handle_t hSrcImage, const ze_image_region_t *pSrcRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyFromMemory(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void *srcptr, const ze_image_region_t *pDstRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryPrefetch(ze_command_list_handle_t hCommandList, const void *ptr, size_t size);
ze_result_t zeCommandListAppendMemAdvise(ze_command_list_handle_t hCommandList, ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_advice_t advice);
ze_result_t zeCommandListAppendSignalEvent(ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent);
ze_result_t zeCommandListAppendWaitOnEvents(ze_command_list_handle_t hCommandList, uint32_t numEvents, ze_event_handle_t *phEvents);
ze_result_t zeCommandListAppendEventReset(ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent);
ze_result_t zeCommandListAppendQueryKernelTimestamps(ze_command_list_handle_t hCommandList, uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr, const size_t *pOffsets, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t kernelHandle, const ze_group_count_t *launchKernelArgs, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchCooperativeKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t kernelHandle, const ze_group_count_t *launchKernelArgs, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchKernelIndirect(ze_command_list_handle_t hCommandList, ze_kernel_handle_t kernelHandle, const ze_group_count_t *pLaunchArgumentsBuffer, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchMultipleKernelsIndirect(ze_command_list_handle_t hCommandList, uint32_t numKernels, ze_kernel_handle_t *kernelHandles, const uint32_t *pCountBuffer, const ze_group_count_t *pLaunchArgumentsBuffer, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendSignalExternalSemaphoreExt(ze_command_list_handle_t hCommandList, uint32_t numSemaphores, ze_external_semaphore_ext_handle_t *phSemaphores, ze_external_semaphore_signal_params_ext_t *signalParams, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendWaitExternalSemaphoreExt(ze_command_list_handle_t hCommandList, uint32_t numSemaphores, ze_external_semaphore_ext_handle_t *phSemaphores, ze_external_semaphore_wait_params_ext_t *waitParams, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyToMemoryExt(ze_command_list_handle_t hCommandList, void *dstptr, ze_image_handle_t hSrcImage, const ze_image_region_t *pSrcRegion, uint32_t destRowPitch, uint32_t destSlicePitch, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyFromMemoryExt(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void *srcptr, const ze_image_region_t *pDstRegion, uint32_t srcRowPitch, uint32_t srcSlicePitch, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);

namespace ult {

struct GraphsCleanupGuard {
    ~GraphsCleanupGuard() {
        processUsesGraphs.store(false);
    }
};

struct MockGraphCmdListWithContext : Mock<CommandList> {
    MockGraphCmdListWithContext(L0::Context *ctx) : ctx(ctx) {}
    ze_result_t getContextHandle(ze_context_handle_t *phContext) override {
        *phContext = ctx;
        return ZE_RESULT_SUCCESS;
    }

    L0::Context *ctx = nullptr;
};

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

TEST(GraphTestDebugApis, WhenIsGraphCaptureEnabledIsCalledThenReturnUnsupportedFeature) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    auto err = ::zeCommandListIsGraphCaptureEnabledExp(&cmdlist);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphTestDebugApis, WhenGraphIsEmptyIsCalledThenReturnUnsupportedFeature) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto err = ::zeGraphIsEmptyExp(&srcGraph);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
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
    ze_image_handle_t imgA = nullptr;
    ze_image_handle_t imgB = nullptr;
    zes_device_handle_t device = nullptr;
    zet_kernel_handle_t kernel = nullptr;
    ze_external_semaphore_ext_handle_t sem = nullptr;
    ze_event_handle_t eventHandle = &event;
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
    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListAppendBarrier(&cmdlist, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListAppendMemoryCopy(&cmdlist, memA, memB, sizeof(memA), nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListAppendWaitOnEvents(&cmdlist, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ASSERT_EQ(3U, graph.getCapturedCommands().size());
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(graph.getCapturedCommands()[0].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(graph.getCapturedCommands()[1].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendWaitOnEvents, static_cast<CaptureApi>(graph.getCapturedCommands()[2].index()));

    // temporarily unsupported
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendWriteGlobalTimestamp(&cmdlist, memA, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendMemoryRangesBarrier(&cmdlist, 1, &rangeSize, &memRange, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendMemoryFill(&cmdlist, memA, memB, 4, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendMemoryCopyRegion(&cmdlist, memA, &copyRegion, 16, 16, memB, &copyRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendMemoryCopyFromContext(&cmdlist, memA, &otherCtx, memB, 4, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendImageCopy(&cmdlist, imgA, imgB, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendImageCopyRegion(&cmdlist, imgA, imgB, &imgRegion, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendImageCopyToMemory(&cmdlist, memA, imgA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendImageCopyFromMemory(&cmdlist, imgA, memA, &imgRegion, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendMemoryPrefetch(&cmdlist, memA, 4));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendMemAdvise(&cmdlist, device, memA, 4, ZE_MEMORY_ADVICE_BIAS_CACHED));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendSignalEvent(&cmdlist, &event));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendEventReset(&cmdlist, &event));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendQueryKernelTimestamps(&cmdlist, 1, &eventHandle, memA, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendLaunchKernel(&cmdlist, kernel, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendLaunchCooperativeKernel(&cmdlist, kernel, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendLaunchKernelIndirect(&cmdlist, kernel, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendLaunchMultipleKernelsIndirect(&cmdlist, 1, &kernel, &kernelCount, &groupCount, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendSignalExternalSemaphoreExt(&cmdlist, 1, &sem, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendWaitExternalSemaphoreExt(&cmdlist, 1, &sem, nullptr, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendImageCopyToMemoryExt(&cmdlist, memA, imgA, &imgRegion, 16, 16, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendImageCopyFromMemoryExt(&cmdlist, imgA, memA, &imgRegion, 16, 16, nullptr, 0, nullptr));
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
    struct MockContextSpecificCmdList : Mock<Context> {
        ze_command_list_handle_t cmdListToReturn = nullptr;
        ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
            *commandList = cmdListToReturn;
            return ZE_RESULT_SUCCESS;
        }
    };

    MockContextSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    Mock<CommandList> subCmdlist;
    Mock<Event> signalEvents[3];
    Mock<Event> waitEvents[3];
    ze_event_handle_t waitEventsList[3] = {&waitEvents[0], &waitEvents[1], &waitEvents[2]};
    std::unique_ptr<Mock<CommandList>> graphHwCommands{new Mock<CommandList>()};
    ctx.cmdListToReturn = graphHwCommands.get();
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
    ASSERT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[0].index()));
    ASSERT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[1].index()));

    GraphInstatiateSettings instantiateAsMonolithic;
    instantiateAsMonolithic.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);
    EXPECT_FALSE(execGraph.isSubGraph());
    EXPECT_FALSE(execGraph.empty());
    EXPECT_TRUE(execGraph.getSubgraphs().empty());
    EXPECT_EQ(1U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyCalled);
    graphHwCommands.release();

    graphHwCommands.reset(new Mock<CommandList>());
    ctx.cmdListToReturn = graphHwCommands.get();

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, signalEvents[0]);
    srcGraph.tryJoinOnNextCommand(subCmdlist, signalEvents[2]);

    ASSERT_EQ(1U, srcGraph.getSubgraphs().size());
    EXPECT_TRUE(srcGraph.getSubgraphs()[0]->isSubGraph());
    EXPECT_TRUE(srcGraph.getSubgraphs()[0]->empty());

    ExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);

    EXPECT_FALSE(execMultiGraph.isSubGraph());
    EXPECT_EQ(1U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyCalled);
    ASSERT_EQ(1U, execMultiGraph.getSubgraphs().size());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->isSubGraph());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->empty());
    graphHwCommands.release();
}

TEST(GraphInstantiation, GivenSourceGraphWhenPolicyIsSetToInterleaveThenExecutableInterleavesParentAndChildCommandListBasedOnForks) {
    GraphsCleanupGuard graphCleanup;
    struct MockContextSpecificCmdList : Mock<Context> {
        ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
            *commandList = new Mock<CommandList>;
            return ZE_RESULT_SUCCESS;
        }
    };

    struct MockExecutableGraph : ExecutableGraph {
        using ExecutableGraph::ExecutableGraph;
        using ExecutableGraph::submissionChain;
    };

    MockContextSpecificCmdList ctx;
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

    struct MockContextSpecificCmdList : Mock<Context> {
        ze_command_list_handle_t cmdListToReturn = nullptr;
        ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
            *commandList = cmdListToReturn;
            return ZE_RESULT_SUCCESS;
        }
    };

    MockContextSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    Mock<Event> event;
    ze_event_handle_t eventHandle = &event;

    uint64_t memA[16] = {};
    uint64_t memB[16] = {};

    L0::Graph srcGraph(&ctx, true);
    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &srcGraph, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListAppendBarrier(&cmdlist, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListAppendMemoryCopy(&cmdlist, memA, memB, sizeof(memA), nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = L0::zeCommandListAppendWaitOnEvents(&cmdlist, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    std::unique_ptr<Mock<CommandList>> graphHwCommands{new Mock<CommandList>()};
    ctx.cmdListToReturn = graphHwCommands.get();
    ExecutableGraph execGraph;

    EXPECT_EQ(0U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(0U, graphHwCommands->appendMemoryCopyCalled);
    EXPECT_EQ(0U, graphHwCommands->appendWaitOnEventsCalled);
    execGraph.instantiateFrom(srcGraph);
    EXPECT_EQ(1U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyCalled);
    EXPECT_EQ(1U, graphHwCommands->appendWaitOnEventsCalled);

    graphHwCommands.release();
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
    struct MockContextSpecificCmdList : Mock<Context> {
        ze_command_list_handle_t cmdListToReturn = nullptr;
        ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
            *commandList = cmdListToReturn;
            return ZE_RESULT_SUCCESS;
        }
    };

    MockContextSpecificCmdList ctx;
    Mock<CommandList> cmdlist;

    std::unique_ptr<Mock<CommandList>> graphHwCommands{new Mock<CommandList>()};
    ctx.cmdListToReturn = graphHwCommands.get();

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&cmdlist, nullptr, 0U, nullptr);
    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);
    graphHwCommands.release();

    auto res = execGraph.execute(&cmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(1U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, cmdlist.appendSignalEventCalled);
}

TEST(GraphExecution, GivenExecutableGraphWithSubGraphsWhenSubmittingItToCommandListSubmitAlsoSubGraphsToRespectiveCommandLists) {
    GraphsCleanupGuard graphCleanup;
    struct MockContextSpecificCmdList : Mock<Context> {
        ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
            *commandList = new Mock<CommandList>;
            return ZE_RESULT_SUCCESS;
        }
    };

    MockContextSpecificCmdList ctx;
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
