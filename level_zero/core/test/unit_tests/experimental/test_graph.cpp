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

TEST(GraphTestApiCreate, GivenNonNullPNextThenGraphCreateReturnsError) {
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
    ze_graph_handle_t graph = nullptr;
    auto err = ::zeGraphCreateExp(nullptr, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graph);
}

TEST(GraphTestApiCreate, GivenValidContextThenGraphCreateReturnsSuccess) {
    Mock<Context> ctx;
    ze_graph_handle_t graph = nullptr;
    auto err = ::zeGraphCreateExp(&ctx, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, graph);

    err = ::zeGraphDestroyExp(graph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCreate, GivenInvalidGraphThenGraphDestroyReturnsError) {
    auto err = ::zeGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphBeginCaptureReturnsError) {
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
    Mock<CommandList> cmdlist;

    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenValidDestinyGraphThenBeginCaptureReturnsSuccessAndOutputGraphIsTheSameAsInput) {
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
    Mock<CommandList> cmdlist;
    ze_graph_handle_t retGraph = nullptr;
    auto err = ::zeCommandListEndGraphCaptureExp(&cmdlist, &retGraph, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureRequiresOutputGraphPlaceholder) {
    Mock<CommandList> cmdlist;
    auto err = ::zeCommandListBeginGraphCaptureExp(&cmdlist, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenDestinyGraphProvidedThenEndCaptureDoesNotRequireOutputGraphPlaceholder) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph graph(&ctx, true);
    auto err = ::zeCommandListBeginCaptureIntoGraphExp(&cmdlist, &graph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = ::zeCommandListEndGraphCaptureExp(&cmdlist, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenCommandListIsAlreadyRecordingThenBeginCaptureReturnsError) {
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
    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = ::zeCommandListInstantiateGraphExp(nullptr, &execGraph, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraph);
}

TEST(GraphTestApiInstantiate, GivenInvalidOutputGraphPlaceholderThenInstantiateGraphReturnsError) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);

    auto err = ::zeCommandListInstantiateGraphExp(&srcGraph, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiInstantiate, GivenNonNullPNextThenInstantiateGraphReturnsError) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = ::zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, &ext);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraph);
}

TEST(GraphTestApiInstantiate, GivenValidSourceGraphThenInstantiateReturnsValidExecutableGraph) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);

    ze_executable_graph_handle_t execGraph = nullptr;
    auto err = ::zeCommandListInstantiateGraphExp(&srcGraph, &execGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, execGraph);

    err = ::zeExecutableGraphDestroyExp(execGraph);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiInstantiate, GivenInvalidExecutableGraphThenGraphDestroyReturnsError) {
    auto err = ::zeExecutableGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestDebugApis, WhenIsGraphCaptureEnabledIsCalledThenReturnUnsupportedFeature) {
    Mock<CommandList> cmdlist;
    auto err = ::zeCommandListIsGraphCaptureEnabledExp(&cmdlist);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphTestDebugApis, WhenGraphIsEmptyIsCalledThenReturnUnsupportedFeature) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto err = ::zeGraphIsEmptyExp(&srcGraph);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphTestDebugApis, WhenGraphDumpContentsIsCalledThenReturnUnsupportedFeature) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto err = ::zeGraphDumpContentsExp(&srcGraph, "dump", nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, err);
}

TEST(GraphTestApiSubmit, GivenNonNullPNextThenGraphAppendReturnsError) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph srcGraph(&ctx, true);
    L0::ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = ::zeCommandListAppendGraphExp(&cmdlist, &execGraph, &ext, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiSubmit, GivenInvalidCmdListThenGraphAppendReturnsError) {
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    L0::ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);
    auto err = ::zeCommandListAppendGraphExp(nullptr, &execGraph, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiSubmit, GivenInvalidGraphThenGraphAppendReturnsError) {
    Mock<CommandList> cmdlist;

    auto err = ::zeCommandListAppendGraphExp(&cmdlist, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiSubmit, GivenValidCmdListAndGraphThenGraphAppendReturnsSuccess) {
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    L0::Graph srcGraph(&ctx, true);
    L0::ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    auto err = ::zeCommandListAppendGraphExp(&cmdlist, &execGraph, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCapture, GivenCommandListInRecordStateThenCaptureCommandsInsteadOfExecutingThem) {
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

    ASSERT_EQ(2U, graph.getCapturedCommands().size());
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(graph.getCapturedCommands()[0].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(graph.getCapturedCommands()[1].index()));

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
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::zeCommandListAppendWaitOnEvents(&cmdlist, 1, &eventHandle));
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

TEST(GraphInstantiation, GivenSourceGraphThenExecutableIsInstantiatedProperly) {
    struct MockContextSpecificCmdList : Mock<Context> {
        ze_command_list_handle_t cmdListToReturn = nullptr;
        ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
            *commandList = cmdListToReturn;
            return ZE_RESULT_SUCCESS;
        }
    };

    MockContextSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    Mock<Event> signalEvents[2];
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

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);
    EXPECT_FALSE(execGraph.isSubGraph());
    EXPECT_FALSE(execGraph.empty());
    EXPECT_TRUE(execGraph.getSubgraphs().empty());
    EXPECT_EQ(1U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyCalled);

    graphHwCommands.release();
    graphHwCommands.reset(new Mock<CommandList>());
    ctx.cmdListToReturn = graphHwCommands.get();

    Graph srcSubGraph(&ctx, true);
    srcSubGraph.startCapturingFrom(cmdlist, true);
    srcSubGraph.stopCapturing();
    srcGraph.addSubGraph(&srcSubGraph);
    ASSERT_EQ(1U, srcGraph.getSubgraphs().size());
    EXPECT_TRUE(srcGraph.getSubgraphs()[0]->isSubGraph());
    EXPECT_TRUE(srcGraph.getSubgraphs()[0]->empty());

    ExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph);

    EXPECT_FALSE(execMultiGraph.isSubGraph());
    EXPECT_EQ(1U, graphHwCommands->appendBarrierCalled);
    EXPECT_EQ(1U, graphHwCommands->appendMemoryCopyCalled);
    ASSERT_EQ(1U, execMultiGraph.getSubgraphs().size());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->isSubGraph());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->empty());
    graphHwCommands.release();
}

TEST(GraphExecution, GivenEmptyExecutableGraphWhenSubmittingItToCommandListThenTakeCareOnlyOfEvents) {
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
    struct MockContextSpecificCmdList : Mock<Context> {
        ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
            *commandList = new Mock<CommandList>;
            return ZE_RESULT_SUCCESS;
        }
    };

    MockContextSpecificCmdList ctx;
    Mock<CommandList> mainRecordCmdlist;
    Mock<CommandList> mainExecCmdlist;
    Mock<CommandList> subCmdlist;

    Graph srcGraph(&ctx, true);
    mainRecordCmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&mainRecordCmdlist, nullptr, 0U, nullptr);
    srcGraph.stopCapturing();
    mainRecordCmdlist.setCaptureTarget(nullptr);

    Graph srcSubGraph(&ctx, true);
    subCmdlist.setCaptureTarget(&srcSubGraph);
    srcSubGraph.startCapturingFrom(subCmdlist, true);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(&subCmdlist, nullptr, 0U, nullptr);
    srcSubGraph.stopCapturing();
    subCmdlist.setCaptureTarget(nullptr);

    srcGraph.addSubGraph(&srcSubGraph);

    ExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph);

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
