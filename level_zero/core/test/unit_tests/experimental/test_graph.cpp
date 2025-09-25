/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/experimental/test_graph.h"

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_graph.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/ze_api.h"

using namespace NEO;

namespace L0 {

namespace ult {

struct GraphFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(context->toHandle(), device->toHandle(), &cmdQueueDesc, &immCmdListHandle));
        immCmdList = L0::CommandList::fromHandle(immCmdListHandle);
    }
    void tearDown() {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(immCmdListHandle));
        DeviceFixture::tearDown();
    }

    ze_command_list_handle_t immCmdListHandle;
    L0::CommandList *immCmdList = nullptr;
};

using GraphTestApiSubmit = Test<GraphFixture>;
using GraphTestApiInstantiate = Test<GraphFixture>;
using GraphTestApiCaptureWithDevice = Test<GraphFixture>;
using GraphTestInstantiationTest = Test<GraphFixture>;
using GraphInstantiation = Test<GraphFixture>;
using GraphExecution = Test<GraphFixture>;

TEST(GraphTestApiCreate, GivenNonNullPNextThenGraphCreateReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    auto ctxHandle = ctx.toHandle();
    ze_graph_handle_t graphHandle = nullptr;
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;
    auto err = zeGraphCreateExp(ctxHandle, &graphHandle, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graphHandle);
}

TEST(GraphTestApiCreate, GivenNullContextThenGraphCreateReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ze_graph_handle_t graphHandle = nullptr;
    auto err = zeGraphCreateExp(nullptr, &graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, graphHandle);
}

TEST(GraphTestApiCreate, GivenValidContextThenGraphCreateReturnsSuccess) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    auto ctxHandle = ctx.toHandle();
    ze_graph_handle_t graphHandle = nullptr;
    auto err = zeGraphCreateExp(ctxHandle, &graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, graphHandle);

    err = zeGraphDestroyExp(graphHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCreate, GivenInvalidGraphThenGraphDestroyReturnsError) {
    GraphsCleanupGuard graphCleanup;
    auto err = zeGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = zeCommandListBeginGraphCaptureExp(cmdListHandle, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);

    L0::Graph graph(&ctx, true);
    auto graphHandle = graph.toHandle();
    err = zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNullDestinyGraphThenBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
}

TEST(GraphTestApiCaptureBeginEnd, GivenValidDestinyGraphThenBeginCaptureReturnsSuccessAndOutputGraphIsTheSameAsInput) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    cmdlist.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    auto cmdListHandle = cmdlist.toHandle();
    L0::Graph graph(&ctx, true);
    auto graphHandle = graph.toHandle();
    auto err = zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, graphHandle);

    EXPECT_EQ(static_cast<ze_command_list_flags_t>(ZE_COMMAND_LIST_FLAG_IN_ORDER), graph.getCaptureTargetDesc().desc.flags);
}

TEST(GraphTestApiCaptureBeginEnd, GivenNonNullPNextThenGraphEndCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, &ext);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureReturnsNewGraph) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, retGraph);

    zeGraphDestroyExp(retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenCommandListIsNotRecordingThenEndCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    ze_graph_handle_t retGraph = nullptr;
    auto err = zeCommandListEndGraphCaptureExp(cmdListHandle, &retGraph, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, retGraph);
}

TEST(GraphTestApiCaptureBeginEnd, WhenNoDestinyGraphProvidedThenEndCaptureRequiresOutputGraphPlaceholder) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = zeCommandListBeginGraphCaptureExp(cmdListHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = zeCommandListEndGraphCaptureExp(cmdListHandle, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenDestinyGraphProvidedThenEndCaptureDoesNotRequireOutputGraphPlaceholder) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    L0::Graph graph(&ctx, true);
    auto graphHandle = graph.toHandle();
    auto err = zeCommandListBeginCaptureIntoGraphExp(cmdListHandle, graphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = zeCommandListEndGraphCaptureExp(cmdListHandle, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST(GraphTestApiCaptureBeginEnd, WhenCommandListIsAlreadyRecordingThenBeginCaptureReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;

    L0::Graph graph1(&ctx, true);
    auto graphHandle1 = graph1.toHandle();
    L0::Graph graph2(&ctx, true);
    auto graphHandle2 = graph2.toHandle();
    auto cmdlistHandle = cmdlist.toHandle();
    auto err = zeCommandListBeginCaptureIntoGraphExp(cmdlistHandle, graphHandle1, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);

    err = zeCommandListBeginCaptureIntoGraphExp(cmdlistHandle, graphHandle2, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    err = zeCommandListBeginGraphCaptureExp(cmdlistHandle, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);

    ze_graph_handle_t retGraph = nullptr;
    err = zeCommandListEndGraphCaptureExp(cmdlistHandle, &retGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(retGraph, graphHandle1);
}

TEST_F(GraphTestApiInstantiate, GivenInvalidSourceGraphThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = zeCommandListInstantiateGraphExp(nullptr, &execGraphHandle, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraphHandle);
}

TEST_F(GraphTestApiInstantiate, GivenInvalidOutputGraphPlaceholderThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    srcGraph.stopCapturing();
    auto srcGraphHandle = srcGraph.toHandle();

    auto err = zeCommandListInstantiateGraphExp(srcGraphHandle, nullptr, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenNonNullPNextThenInstantiateGraphReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto srcGraphHandle = srcGraph.toHandle();
    srcGraph.stopCapturing();

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, &ext);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, execGraphHandle);
}

TEST_F(GraphTestApiInstantiate, GivenValidSourceGraphThenInstantiateReturnsValidExecutableGraph) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;

    L0::Graph srcGraph(&ctx, true);
    srcGraph.startCapturingFrom(*immCmdList, false);
    auto srcGraphHandle = srcGraph.toHandle();
    srcGraph.stopCapturing();

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_NE(nullptr, execGraphHandle);

    err = zeExecutableGraphDestroyExp(execGraphHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenInvalidExecutableGraphThenGraphDestroyReturnsError) {
    GraphsCleanupGuard graphCleanup;
    auto err = zeExecutableGraphDestroyExp(nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiInstantiate, GivenUnclosedGraphThenInstantiateFails) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    L0::Graph srcGraph(&ctx, true);
    auto srcGraphHandle = srcGraph.toHandle();

    ze_executable_graph_handle_t execGraphHandle = nullptr;
    auto err = zeCommandListInstantiateGraphExp(srcGraphHandle, &execGraphHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
    EXPECT_EQ(nullptr, execGraphHandle);
}

TEST(GraphTestDebugApis, GivenNullGraphWhenIsGraphCaptureEnabledIsCalledThenReturnError) {
    GraphsCleanupGuard graphCleanup;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeCommandListIsGraphCaptureEnabledExp(nullptr));
}

TEST(GraphTestDebugApis, GivenCommandListWithoutCaptureEnabledWhenIsGraphCaptureEnabledIsCalledThenReturnFalse) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    auto cmdListHandle = cmdlist.toHandle();
    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, zeCommandListIsGraphCaptureEnabledExp(cmdListHandle));
}

TEST(GraphTestDebugApis, GivenCommandListWithCaptureEnabledWhenIsGraphCaptureEnabledIsCalledThenReturnTrue) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    auto cmdListHandle = cmdlist.toHandle();

    Graph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);

    EXPECT_EQ(ZE_RESULT_QUERY_TRUE, zeCommandListIsGraphCaptureEnabledExp(cmdListHandle));

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 0U, nullptr);

    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, zeCommandListIsGraphCaptureEnabledExp(cmdListHandle));
}

TEST(GraphTestDebugApis, GivenNullGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zeGraphIsEmptyExp(nullptr));
}

TEST(GraphTestDebugApis, GivenInvalidGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();

    Graph srcGraph(&ctx, true);
    auto srcGraphHandle = srcGraph.toHandle();
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, forkEventHandle, 0U, nullptr);

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, forkEvent);
    srcSubGraph->stopCapturing();
    subCmdlist.setCaptureTarget(nullptr);
    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GRAPH, zeGraphIsEmptyExp(srcGraphHandle));
}

TEST(GraphTestDebugApis, GivenEmptyGraphWhenGraphIsEmptyIsCalledThenTrue) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Graph srcGraph(&ctx, true);
    auto srcGraphHandle = srcGraph.toHandle();

    EXPECT_EQ(ZE_RESULT_QUERY_TRUE, zeGraphIsEmptyExp(srcGraphHandle));
}

TEST(GraphTestDebugApis, GivenNonEmptyGraphWhenGraphIsEmptyIsCalledThenErrorIsReturned) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    auto cmdListHandle = cmdlist.toHandle();

    Graph srcGraph(&ctx, true);
    auto srcGraphHandle = srcGraph.toHandle();
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);

    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 0U, nullptr);

    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    EXPECT_EQ(ZE_RESULT_QUERY_FALSE, zeGraphIsEmptyExp(srcGraphHandle));
}

TEST_F(GraphTestApiSubmit, GivenNonNullPNextThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    cmdlist.device = this->device;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    auto execGraphHandle = execGraph.toHandle();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    execGraph.instantiateFrom(srcGraph);

    ze_base_desc_t ext = {};
    ext.stype = ZE_STRUCTURE_TYPE_MUTABLE_GRAPH_ARGUMENT_EXP_DESC;
    ext.pNext = nullptr;

    auto err = zeCommandListAppendGraphExp(cmdListHandle, execGraphHandle, &ext, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiSubmit, GivenInvalidCmdListThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    MockGraph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    auto execGraphHandle = execGraph.toHandle();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    execGraph.instantiateFrom(srcGraph);
    auto err = zeCommandListAppendGraphExp(nullptr, execGraphHandle, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiSubmit, GivenInvalidGraphThenGraphAppendReturnsError) {
    GraphsCleanupGuard graphCleanup;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();

    auto err = zeCommandListAppendGraphExp(cmdListHandle, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiSubmit, GivenValidCmdListAndGraphThenGraphAppendReturnsSuccess) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    MockGraph srcGraph(&ctx, true);
    srcGraph.stopCapturing();

    L0::ExecutableGraph execGraph;
    auto execGraphHandle = execGraph.toHandle();
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    execGraph.instantiateFrom(srcGraph);

    auto err = zeCommandListAppendGraphExp(cmdListHandle, execGraphHandle, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
}

TEST_F(GraphTestApiCaptureWithDevice, GivenCommandListInRecordStateThenCaptureCommandsInsteadOfExecutingThem) {
    GraphsCleanupGuard graphCleanup;
    Mock<Context> ctx;
    Mock<Context> otherCtx;
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

    L0::Graph graph(&ctx, true);
    ze_graph_handle_t graphHandle = graph.toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, graphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(immCmdListHandle, memA, memB, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(immCmdListHandle, 1, &eventHandle));
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

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListEndGraphCaptureExp(immCmdListHandle, &graphHandle, nullptr));

    ASSERT_EQ(27U, graph.getCapturedCommands().size());
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
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchCooperativeKernel, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelIndirect, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelWithParameters, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendLaunchKernelWithArguments, static_cast<CaptureApi>(graph.getCapturedCommands()[i++].index()));
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
    ze_event_handle_t regularEventHandle = regularEvent.toHandle();
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    auto err = zeCommandListAppendWaitOnEvents(cmdListHandle, 1, &regularEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, err);
    EXPECT_EQ(nullptr, cmdlist.getCaptureTarget());
}

TEST_F(GraphInstantiation, GivenSourceGraphThenExecutableIsInstantiatedProperly) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();
    Mock<CommandList> subCmdlist;
    Mock<Event> signalEvents[3];
    Mock<Event> waitEvents[3];
    ze_event_handle_t waitEventsList[3] = {waitEvents[0].toHandle(), waitEvents[1].toHandle(), waitEvents[2].toHandle()};
    ctx.cmdListToReturn = new Mock<CommandList>();
    auto *graphCmdList = ctx.cmdListToReturn;
    uint64_t memA[16] = {};
    uint64_t memB[16] = {};

    MockGraph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, signalEvents[0].toHandle(), 2U, waitEventsList);
    cmdlist.capture<CaptureApi::zeCommandListAppendMemoryCopy>(cmdListHandle, memA, memB, sizeof(memA), signalEvents[1].toHandle(), 3U, waitEventsList);
    srcGraph.stopCapturing();
    EXPECT_FALSE(srcGraph.isSubGraph());

    ASSERT_EQ(2U, srcGraph.getCapturedCommands().size());
    EXPECT_EQ(CaptureApi::zeCommandListAppendBarrier, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[0].index()));
    EXPECT_EQ(CaptureApi::zeCommandListAppendMemoryCopy, static_cast<CaptureApi>(srcGraph.getCapturedCommands()[1].index()));

    GraphInstatiateSettings instantiateAsMonolithic;
    instantiateAsMonolithic.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
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

    static_cast<MockGraph *>(srcSubGraph)->captureTargetDesc.hDevice = device->toHandle();

    ExecutableGraph execMultiGraph;
    execMultiGraph.instantiateFrom(srcGraph, instantiateAsMonolithic);

    EXPECT_FALSE(execMultiGraph.isSubGraph());
    EXPECT_EQ(1U, graphCmdList->appendBarrierCalled);
    EXPECT_EQ(1U, graphCmdList->appendMemoryCopyCalled);
    ASSERT_EQ(1U, execMultiGraph.getSubgraphs().size());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->isSubGraph());
    EXPECT_TRUE(execMultiGraph.getSubgraphs()[0]->empty());
}

TEST_F(GraphInstantiation, GivenSourceGraphWhenPolicyIsSetToInterleaveThenExecutableInterleavesParentAndChildCommandListBasedOnForks) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext cmdlist{&ctx};
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext subCmdlist{&ctx};
    Mock<Event> forkEvent;
    Mock<Event> joinEvent;
    ze_event_handle_t forkEventHandle = forkEvent.toHandle();
    ze_event_handle_t joinEventHandle = joinEvent.toHandle();

    MockGraph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
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
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> joinEvent;
    { // missing join
        Graph srcGraph(&ctx, true);
        Graph *srcGraphPtr = &srcGraph;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, cmdListHandle, forkEventHandle, 0U, nullptr);
        Graph *childGraph = nullptr;
        srcGraph.forkTo(childCmdlist, childGraph, forkEvent);
        srcGraph.stopCapturing();
        EXPECT_FALSE(srcGraph.validForInstantiation());
        childCmdlist.setCaptureTarget(nullptr);
    }

    { // correct graph
        Graph srcGraph(&ctx, true);
        Graph *srcGraphPtr = &srcGraph;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, cmdListHandle, forkEventHandle, 0U, nullptr);
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
    auto cmdListHandle = cmdlist.toHandle();
    MockGraphCmdListWithContext childCmdlist{&ctx};
    MockGraphCmdListWithContext grandChildCmdlist{&ctx};
    Mock<Event> forkEvent;
    auto forkEventHandle = forkEvent.toHandle();
    Mock<Event> forkEventLvl2;
    auto forkEventHandleLvl2 = forkEventLvl2.toHandle();
    Mock<Event> joinEvent;
    Mock<Event> joinEventLvl2;
    auto joinEventHandleLvl2 = joinEventLvl2.toHandle();
    { // missing join
        Graph srcGraph(&ctx, true);
        Graph *srcGraphPtr = &srcGraph;
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, cmdListHandle, forkEventHandle, 0U, nullptr);
        Graph *childGraph = nullptr;
        srcGraph.forkTo(childCmdlist, childGraph, forkEvent);

        {
            Graph *grandChildGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, &childCmdlist, forkEventHandleLvl2, 0U, nullptr);
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
        L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(cmdlist, srcGraphPtr, cmdListHandle, forkEventHandle, 0U, nullptr);
        Graph *childGraph = nullptr;
        srcGraph.forkTo(childCmdlist, childGraph, forkEvent);

        {
            Graph *grandChildGraph = nullptr;
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(childCmdlist, childGraph, &childCmdlist, forkEventHandleLvl2, 0U, nullptr);
            childGraph->forkTo(grandChildCmdlist, grandChildGraph, forkEventLvl2);
            L0::captureCommand<CaptureApi::zeCommandListAppendBarrier>(grandChildCmdlist, grandChildGraph, &grandChildCmdlist, joinEventHandleLvl2, 0U, nullptr);
            childGraph->tryJoinOnNextCommand(grandChildCmdlist, joinEventLvl2);
        }

        srcGraph.tryJoinOnNextCommand(childCmdlist, joinEvent);
        srcGraph.stopCapturing();
        EXPECT_TRUE(srcGraph.validForInstantiation());
    }
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

    L0::Graph srcGraph(&ctx, true);
    auto srcGraphHandle = srcGraph.toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListBeginCaptureIntoGraphExp(immCmdListHandle, srcGraphHandle, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(immCmdListHandle, memA, memB, sizeof(memA), nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendWaitOnEvents(immCmdListHandle, 1, &eventHandle));
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

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListEndGraphCaptureExp(immCmdListHandle, &srcGraphHandle, nullptr));

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
    EXPECT_EQ(0U, graphHwCommands->appendLaunchKernelIndirectCalled);
    EXPECT_EQ(0U, graphHwCommands->appendLaunchMultipleKernelsIndirectCalled);
    EXPECT_EQ(0U, graphHwCommands->appendLaunchKernelWithParametersCalled);
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
    EXPECT_EQ(2U, graphHwCommands->appendLaunchKernelCalled); // +1 for zeCommandListAppendLaunchCooperativeKernel
    EXPECT_EQ(1U, graphHwCommands->appendLaunchKernelIndirectCalled);
    EXPECT_EQ(1U, graphHwCommands->appendLaunchMultipleKernelsIndirectCalled);
    EXPECT_EQ(2U, graphHwCommands->appendLaunchKernelWithParametersCalled); // +1 for zeCommandListAppendLaunchKernelWithArguments
}

TEST_F(GraphTestInstantiationTest, givenInOrderCmdListAndRegularCbEventWhenInstantiateToGraphThenDoNotRecordExternalCbEvent) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    zex_counter_based_event_desc_t eventDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    eventDesc.flags = static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    auto event = L0::Event::fromHandle(eventHandle);
    auto &externalCbEventContainer = execGraph.getExternalCbEventInfoContainer().getCbEventInfos();
    EXPECT_EQ(0u, externalCbEventContainer.size());
    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest, givenInOrderCmdListAndExternalCbEventWhenInstantiateToGraphThenRecordExternalCbEvent) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    zex_counter_based_event_desc_t eventDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    eventDesc.flags = static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_GRAPH_EXTERNAL_EVENT);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    auto event = L0::Event::fromHandle(eventHandle);
    auto &externalCbEventContainer = execGraph.getExternalCbEventInfoContainer().getCbEventInfos();
    // event used twice in the graph, but should be recorded only once
    EXPECT_EQ(1u, externalCbEventContainer.size());
    bool externalCbEventFound = false;
    for (const auto &entry : externalCbEventContainer) {
        if (event == entry.event) {
            externalCbEventFound = true;
            break;
        }
    }
    EXPECT_TRUE(externalCbEventFound);
    srcGraph.reset();
    event->destroy();
}

TEST_F(GraphTestInstantiationTest, givenInOrderCmdListAndExternalCbEventWhenExecuteTwoGraphsThenRecordedExternalCbEventAttachedToLatestExecutedGraph) {
    GraphsCleanupGuard graphCleanup;

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    commandList->setOrdinal(0);
    auto commandListHandle = commandList->toHandle();

    ze_event_handle_t eventHandle = nullptr;
    zex_counter_based_event_desc_t eventDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    eventDesc.flags = static_cast<uint32_t>(ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_GRAPH_EXTERNAL_EVENT);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context->toHandle(), device->toHandle(), &eventDesc, &eventHandle));

    std::unique_ptr<L0::Graph> srcGraph = std::make_unique<L0::Graph>(context, true);
    ze_graph_handle_t graphHandle = srcGraph->toHandle();

    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListBeginCaptureIntoGraphExp(commandListHandle, graphHandle, nullptr));

    Mock<Module> module(this->device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_handle_t kernelHandle = kernel.toHandle();
    ze_group_count_t groupCount = {1, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(commandListHandle, kernelHandle, &groupCount, eventHandle, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListEndGraphCaptureExp(commandListHandle, &graphHandle, nullptr));

    auto event = L0::Event::fromHandle(eventHandle);

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(*(srcGraph.get()));

    NEO::InOrderExecInfo *graphExecInfo = nullptr;
    auto &externalCbEventContainer = execGraph.getExternalCbEventInfoContainer().getCbEventInfos();
    for (const auto &entry : externalCbEventContainer) {
        if (event == entry.event) {
            graphExecInfo = entry.eventSharedPtrInfo.lock().get();
            break;
        }
    }
    EXPECT_NE(nullptr, graphExecInfo);

    // 1st executable graph, event attached during instantiation
    NEO::InOrderExecInfo *currentEventInOrderExecInfo = event->getInOrderExecInfo().get();
    EXPECT_EQ(currentEventInOrderExecInfo, graphExecInfo);

    ExecutableGraph execGraph2;
    execGraph2.instantiateFrom(*(srcGraph.get()));

    NEO::InOrderExecInfo *graph2ExecInfo = nullptr;
    auto &externalCbEventContainer2 = execGraph2.getExternalCbEventInfoContainer().getCbEventInfos();
    for (const auto &entry : externalCbEventContainer2) {
        if (event == entry.event) {
            graph2ExecInfo = entry.eventSharedPtrInfo.lock().get();
            break;
        }
    }
    EXPECT_NE(nullptr, graph2ExecInfo);
    EXPECT_NE(graphExecInfo, graph2ExecInfo);

    // 2nd executable graph, event attached during instantiation
    currentEventInOrderExecInfo = event->getInOrderExecInfo().get();
    EXPECT_EQ(currentEventInOrderExecInfo, graph2ExecInfo);

    // 1st executable graph execution, event should be attached to 1st graphExecInfo
    returnValue = execGraph.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    currentEventInOrderExecInfo = event->getInOrderExecInfo().get();
    EXPECT_EQ(currentEventInOrderExecInfo, graphExecInfo);

    // 2nd executable graph execution, event should be attached to 2nd graphExecInfo
    returnValue = execGraph2.execute(commandList.get(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    currentEventInOrderExecInfo = event->getInOrderExecInfo().get();
    EXPECT_EQ(currentEventInOrderExecInfo, graph2ExecInfo);

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

    Graph srcGraph(context, true);
    immCmdListHw->setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(*immCmdListHw, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0U, nullptr));
    srcGraph.stopCapturing();
    immCmdListHw->setCaptureTarget(nullptr);

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
            GivenExecutableGraphWithMultipleEnginesWhenSubmittingItToCommandListThenSynchronizedPatchPreambleIsUsed) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(64);

    GraphsCleanupGuard graphCleanup;

    auto immCmdListHw = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(L0::CommandList::fromHandle(immCmdListHandle));
    Graph srcGraph(context, true);
    immCmdListHw->setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(*immCmdListHw, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(immCmdListHandle, nullptr, 0U, nullptr));
    srcGraph.stopCapturing();
    immCmdListHw->setCaptureTarget(nullptr);

    MockExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    ASSERT_TRUE(execGraph.multiEngineGraph);
    ASSERT_NE(0u, execGraph.myCommandLists.size());

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
}

TEST_F(GraphExecution, GivenEmptyExecutableGraphWhenSubmittingItToCommandListThenTakeCareOnlyOfEvents) {
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
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, cmdlist.appendSignalEventCalled);

    res = graph.execute(&cmdlist, nullptr, signalEvents[0].toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(0U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(1U, cmdlist.appendSignalEventCalled);

    res = graph.execute(&cmdlist, nullptr, signalEvents[0].toHandle(), 2, waitEventsList);
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

TEST_F(GraphExecution, GivenExecutableGraphWhenSubmittingItToCommandListThenAppendIt) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    Mock<CommandList> cmdlist;
    auto cmdListHandle = cmdlist.toHandle();

    ctx.cmdListToReturn = new Mock<CommandList>();

    MockGraph srcGraph(&ctx, true);
    cmdlist.setCaptureTarget(&srcGraph);
    srcGraph.startCapturingFrom(cmdlist, false);
    cmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(cmdListHandle, nullptr, 0U, nullptr);
    srcGraph.stopCapturing();
    cmdlist.setCaptureTarget(nullptr);

    srcGraph.captureTargetDesc.hDevice = device->toHandle();

    ExecutableGraph execGraph;
    execGraph.instantiateFrom(srcGraph);

    auto res = execGraph.execute(&cmdlist, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(1U, cmdlist.appendCommandListsCalled);
    EXPECT_EQ(0U, cmdlist.appendWaitOnEventsCalled);
    EXPECT_EQ(0U, cmdlist.appendSignalEventCalled);
}

TEST_F(GraphExecution, GivenExecutableGraphWithSubGraphsWhenSubmittingItToCommandListSubmitAlsoSubGraphsToRespectiveCommandLists) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningNewCmdList ctx;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true);
    mainRecordCmdlist.setCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, signalEventParent);
    subCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(subCmdlistHandle, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    static_cast<MockGraph *>(srcSubGraph)->captureTargetDesc.hDevice = device->toHandle();
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

TEST_F(GraphExecution, GivenGraphWithForkWhenInstantiatingToExecutableAndAppendReturnFailThenReturnCorrectErrorCode) {
    GraphsCleanupGuard graphCleanup;

    MockGraphContextReturningSpecificCmdList ctx;
    ctx.cmdListToReturn = new Mock<CommandList>();
    ctx.cmdListToReturn->appendWriteGlobalTimestampResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    MockGraphCmdListWithContext mainRecordCmdlist{&ctx};
    MockGraphCmdListWithContext mainExecCmdlist{&ctx};
    MockGraphCmdListWithContext subCmdlist{&ctx};
    auto subCmdlistHandle = subCmdlist.toHandle();

    Mock<Event> signalEventParent; // fork
    Mock<Event> signalEventChild;  // join

    ze_event_handle_t signalEventParentHandle = signalEventParent.toHandle();
    ze_event_handle_t signalEventChildHandle = signalEventChild.toHandle();

    MockGraph srcGraph(&ctx, true);
    mainRecordCmdlist.setCaptureTarget(&srcGraph);
    auto mainRecordCmdlistHandle = mainRecordCmdlist.toHandle();
    srcGraph.startCapturingFrom(mainRecordCmdlist, false);
    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, signalEventParentHandle, 0U, nullptr);

    Graph *srcSubGraph = nullptr;
    srcGraph.forkTo(subCmdlist, srcSubGraph, signalEventParent);
    uint64_t *dstptr = nullptr;
    subCmdlist.capture<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(subCmdlistHandle, dstptr, signalEventChildHandle, 1U, &signalEventParentHandle);

    mainRecordCmdlist.capture<CaptureApi::zeCommandListAppendBarrier>(mainRecordCmdlistHandle, nullptr, 1U, &signalEventChildHandle);

    srcGraph.stopCapturing();
    mainRecordCmdlist.setCaptureTarget(nullptr);

    ExecutableGraph execMultiGraph;
    GraphInstatiateSettings settings;
    settings.forkPolicy = GraphInstatiateSettings::ForkPolicyMonolythicLevels;
    srcGraph.captureTargetDesc.hDevice = device->toHandle();
    static_cast<MockGraph *>(srcSubGraph)->captureTargetDesc.hDevice = device->toHandle();

    auto srcGraphHandle = srcGraph.toHandle();

    ze_executable_graph_handle_t physicalGraph = nullptr;
    auto err = zeCommandListInstantiateGraphExp(srcGraphHandle, &physicalGraph, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, err);
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

} // namespace ult
} // namespace L0
