/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submission_status.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/command_queue/blit_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

namespace NEO {
class ExecutionEnvironment;
class GraphicsAllocation;
struct BatchBuffer;
template <typename GfxFamily>
class MockCommandQueueHw;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

using BlitAuxTranslationTests = BlitEnqueueTests<1>;

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWhenConstructingCommandBufferThenEnsureCorrectOrder) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    auto initialBcsTaskCount = mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType());

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    EXPECT_EQ(initialBcsTaskCount + 1, mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType()));

    // Gpgpu command buffer
    {
        auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
        auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

        // Barrier
        expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        // Walker
        cmdFound = expectCommand<DefaultWalkerType>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<DefaultWalkerType>(++cmdFound, cmdListQueue.end());
        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());

        // task count
        expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());
    }

    // BCS command buffer
    {
        auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

        // Barrier
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

        // Aux to NonAux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // wait for NDR (walker split)
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());

        // NonAux to Aux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // taskCount
        expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
    }
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenGpuHangOnFlushBcsAndBlitAuxTranslationWhenConstructingCommandBufferThenOutOfResourcesIsReturned) {
    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);
    ultBcsCsr->callBaseFlushBcsTask = false;
    ultBcsCsr->flushBcsTaskReturnValue = CompletionStamp::gpuHang;

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());

    const auto result = mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, result);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWhenConstructingBlockedCommandBufferThenEnsureCorrectOrder) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    auto initialBcsTaskCount = mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType());

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(initialBcsTaskCount + 1, mockCmdQ->peekBcsTaskCount(bcsCsr->getOsContext().getEngineType()));

    // Gpgpu command buffer
    {
        auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
        auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
        auto cmdListQueue = getCmdList<FamilyType>(*ultCsr->lastFlushedCommandStream, 0);

        // Barrier
        expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        // Walker
        cmdFound = expectCommand<DefaultWalkerType>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<DefaultWalkerType>(++cmdFound, cmdListQueue.end());
        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());

        // task count
        expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());
    }

    // BCS command buffer
    {
        auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

        // Barrier
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

        // Aux to NonAux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // wait for NDR (walker split)
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());

        // NonAux to Aux
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
        cmdFound = expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());

        // taskCount
        expectCommand<MI_FLUSH_DW>(++cmdFound, cmdList.end());
    }
    EXPECT_FALSE(mockCmdQ->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeBarrier) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
    auto pipeControl = expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);

    uint64_t barrierGpuAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());
    verifySemaphore<FamilyType>(semaphore, barrierGpuAddress);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, whenFlushTagUpdateThenFenceAndMiFlushDwIsFlushed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    EXPECT_EQ(SubmissionStatus::success, bcsCsr->flushTagUpdate());

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

    auto beginItor = cmdListBcs.begin();

    if constexpr (FamilyType::isUsingMiMemFence) {
        beginItor = expectCommand<typename FamilyType::MI_MEM_FENCE>(cmdListBcs.begin(), cmdListBcs.end());
        EXPECT_NE(beginItor, cmdListBcs.end());
    }

    auto cmdFound = expectCommand<MI_FLUSH_DW>(beginItor, cmdListBcs.end());
    EXPECT_NE(cmdFound, cmdListBcs.end());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, whenFlushTagUpdateThenSetStallingCmdsFlag) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    ultCsr->recordFlushedBatchBuffer = true;

    EXPECT_EQ(SubmissionStatus::success, bcsCsr->flushTagUpdate());

    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, whenInitializeDeviceWithFirstSubmissionThenMiFlushDwIsFlushed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    EXPECT_EQ(SubmissionStatus::success, bcsCsr->initializeDeviceWithFirstSubmission(device->getDevice()));

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

    auto cmdFound = expectCommand<MI_FLUSH_DW>(cmdListBcs.begin(), cmdListBcs.end());
    EXPECT_NE(cmdFound, cmdListBcs.end());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeBcsOutput) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 2>{{buffer0.get(), buffer1.get()}});

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    uint64_t auxToNonAuxOutputAddress[2] = {};
    uint64_t nonAuxToAuxOutputAddress[2] = {};
    {
        auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

        auto cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        auto miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();
    }
    auto gmmHelper = device->getGmmHelper();
    nonAuxToAuxOutputAddress[0] = gmmHelper->canonize(nonAuxToAuxOutputAddress[0]);
    nonAuxToAuxOutputAddress[1] = gmmHelper->canonize(nonAuxToAuxOutputAddress[1]);
    auxToNonAuxOutputAddress[0] = gmmHelper->canonize(auxToNonAuxOutputAddress[0]);
    auxToNonAuxOutputAddress[1] = gmmHelper->canonize(auxToNonAuxOutputAddress[1]);

    {
        auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[1]);

        // Walker
        cmdFound = expectCommand<DefaultWalkerType>(++cmdFound, cmdListQueue.end());

        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[1]);
    }
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeKernel) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = false;

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto kernelNode = mockCmdQ->timestampPacketContainer->peekNodes()[0];
    auto kernelNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*kernelNode);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

    // Aux to nonAux
    auto cmdFound = expectCommand<XY_COPY_BLT>(cmdList.begin(), cmdList.end());

    // semaphore before NonAux to Aux
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    verifySemaphore<FamilyType>(semaphore, kernelNodeAddress);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeCacheFlush) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = true;
    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto cmdListQueue = getCmdList<FamilyType>(mockCmdQ->getCS(0), 0);

    uint64_t cacheFlushWriteAddress = 0;

    {
        auto cmdFound = expectCommand<DefaultWalkerType>(cmdListQueue.begin(), cmdListQueue.end());
        cmdFound = expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());

        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdFound);
        if (!pipeControlCmd->getDcFlushEnable()) {
            // skip pipe control with TimestampPacket write
            cmdFound = expectPipeControl<FamilyType>(++cmdFound, cmdListQueue.end());
            pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdFound);
        }

        EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
        EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
        cacheFlushWriteAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
        EXPECT_NE(0u, cacheFlushWriteAddress);
    }

    {
        // Aux to nonAux
        auto cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());

        // semaphore before NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListBcs.end());
        verifySemaphore<FamilyType>(cmdFound, cacheFlushWriteAddress);
    }
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingCommandBufferThenSynchronizeEvents) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto event = makeReleaseable<Event>(commandQueue.get(), CL_COMMAND_READ_BUFFER, 0, 0);
    MockTimestampPacketContainer eventDependencyContainer(*bcsCsr->getTimestampPacketAllocator(), 1);
    auto eventDependency = eventDependencyContainer.getNode(0);
    event->addTimestampPacketNodes(eventDependencyContainer);
    cl_event clEvent[] = {event.get()};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, clEvent, nullptr);

    auto eventDependencyAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventDependency);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

    // Barrier
    auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

    // Event
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    verifySemaphore<FamilyType>(semaphore, eventDependencyAddress);

    cmdFound = expectCommand<XY_COPY_BLT>(++semaphore, cmdList.end());
    expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenOutEventWhenDispatchingThenAssignNonAuxNodes) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);

    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    cl_event clEvent;
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &clEvent);
    auto event = castToObject<Event>(clEvent);
    auto &eventNodes = event->getTimestampPacketNodes()->peekNodes();
    EXPECT_EQ(5u, eventNodes.size());

    auto cmdListQueue = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

    auto cmdFound = expectCommand<DefaultWalkerType>(cmdListQueue.begin(), cmdListQueue.end());

    // NonAux to Aux
    cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
    auto eventNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventNodes[1]);
    verifySemaphore<FamilyType>(cmdFound, eventNodeAddress);

    cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
    eventNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventNodes[2]);
    verifySemaphore<FamilyType>(cmdFound, eventNodeAddress);

    EXPECT_NE(0u, event->peekBcsTaskCountFromCommandQueue());

    clReleaseEvent(clEvent);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWhenDispatchingThenEstimateCmdBufferSize) {
    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);

    KernelObjsForAuxTranslation kernelObjects;
    kernelObjects.insert({KernelObjForAuxTranslation::Type::memObj, buffer0.get()});
    kernelObjects.insert({KernelObjForAuxTranslation::Type::memObj, buffer2.get()});

    size_t numBuffersToEstimate = 2;
    size_t dependencySize = numBuffersToEstimate * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<FamilyType>();

    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    mockCmdQ->storeMultiDispatchInfo = true;
    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    MultiDispatchInfo &multiDispatchInfo = mockCmdQ->storedMultiDispatchInfo;
    DispatchInfo *firstDispatchInfo = multiDispatchInfo.begin();
    DispatchInfo *lastDispatchInfo = &(*multiDispatchInfo.rbegin());

    EXPECT_NE(firstDispatchInfo, lastDispatchInfo); // walker split

    auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();
    EXPECT_EQ(dependencySize, firstDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(0u, firstDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));

    EXPECT_EQ(0u, lastDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(dependencySize, lastDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitAuxTranslationWithRequiredCacheFlushWhenDispatchingThenEstimateCmdBufferSize) {
    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQ->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCmdQ->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, false);
    auto buffer2 = createBuffer(1, true);

    KernelObjsForAuxTranslation kernelObjects;
    kernelObjects.insert({KernelObjForAuxTranslation::Type::memObj, buffer0.get()});
    kernelObjects.insert({KernelObjForAuxTranslation::Type::memObj, buffer2.get()});

    size_t numBuffersToEstimate = 2;
    size_t dependencySize = numBuffersToEstimate * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<FamilyType>();

    auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();
    size_t cacheFlushSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);

    setMockKernelArgs(std::array<Buffer *, 3>{{buffer0.get(), buffer1.get(), buffer2.get()}});

    mockCmdQ->storeMultiDispatchInfo = true;
    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, lws, 0, nullptr, nullptr);

    MultiDispatchInfo &multiDispatchInfo = mockCmdQ->storedMultiDispatchInfo;
    DispatchInfo *firstDispatchInfo = multiDispatchInfo.begin();
    DispatchInfo *lastDispatchInfo = &(*multiDispatchInfo.rbegin());

    EXPECT_NE(firstDispatchInfo, lastDispatchInfo); // walker split

    EXPECT_EQ(dependencySize, firstDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(0u, firstDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));

    EXPECT_EQ(0u, lastDispatchInfo->dispatchInitCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));
    EXPECT_EQ(dependencySize + cacheFlushSize, lastDispatchInfo->dispatchEpilogueCommands.estimateCommandsSize(kernelObjects.size(), rootDeviceEnvironment, mockCmdQ->isCacheFlushForBcsRequired()));
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeBarrier) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto cmdListCsr = getCmdList<FamilyType>(gpgpuCsr->getCS(0), 0);
    auto pipeControl = expectPipeControl<FamilyType>(cmdListCsr.begin(), cmdListCsr.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControl);

    uint64_t barrierGpuAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());
    verifySemaphore<FamilyType>(semaphore, barrierGpuAddress);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeEvents) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto event = makeReleaseable<Event>(commandQueue.get(), CL_COMMAND_READ_BUFFER, 0, 0);
    MockTimestampPacketContainer eventDependencyContainer(*bcsCsr->getTimestampPacketAllocator(), 1);
    auto eventDependency = eventDependencyContainer.getNode(0);
    event->addTimestampPacketNodes(eventDependencyContainer);

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent, event.get()};
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 2, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto eventDependencyAddress = TimestampPacketHelper::getContextEndGpuAddress(*eventDependency);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

    // Barrier
    auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdList.begin(), cmdList.end());

    // Event
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    verifySemaphore<FamilyType>(semaphore, eventDependencyAddress);

    cmdFound = expectCommand<XY_COPY_BLT>(++semaphore, cmdList.end());
    expectCommand<XY_COPY_BLT>(++cmdFound, cmdList.end());

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeKernel) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    mockCmdQ->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto kernelNode = mockCmdQ->timestampPacketContainer->peekNodes()[0];
    auto kernelNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*kernelNode);

    auto cmdList = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

    // Aux to nonAux
    auto cmdFound = expectCommand<XY_COPY_BLT>(cmdList.begin(), cmdList.end());

    // semaphore before NonAux to Aux
    auto semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdList.end());
    if (mockCmdQ->isCacheFlushForBcsRequired()) {
        semaphore = expectCommand<MI_SEMAPHORE_WAIT>(++semaphore, cmdList.end());
    }
    verifySemaphore<FamilyType>(semaphore, kernelNodeAddress);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenConstructingBlockedCommandBufferThenSynchronizeBcsOutput) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 2>{{buffer0.get(), buffer1.get()}});

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    uint64_t auxToNonAuxOutputAddress[2] = {};
    uint64_t nonAuxToAuxOutputAddress[2] = {};
    {
        auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);

        auto cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        auto miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        auxToNonAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectCommand<XY_COPY_BLT>(++cmdFound, cmdListBcs.end());

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[0] = miflushDwCmd->getDestinationAddress();

        cmdFound = expectMiFlush<MI_FLUSH_DW>(++cmdFound, cmdListBcs.end());
        miflushDwCmd = genCmdCast<MI_FLUSH_DW *>(*cmdFound);
        nonAuxToAuxOutputAddress[1] = miflushDwCmd->getDestinationAddress();
    }
    auto gmmHelper = device->getGmmHelper();
    nonAuxToAuxOutputAddress[0] = gmmHelper->canonize(nonAuxToAuxOutputAddress[0]);
    nonAuxToAuxOutputAddress[1] = gmmHelper->canonize(nonAuxToAuxOutputAddress[1]);
    auxToNonAuxOutputAddress[0] = gmmHelper->canonize(auxToNonAuxOutputAddress[0]);
    auxToNonAuxOutputAddress[1] = gmmHelper->canonize(auxToNonAuxOutputAddress[1]);

    {
        auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
        auto cmdListQueue = getCmdList<FamilyType>(*ultCsr->lastFlushedCommandStream, 0);

        // Aux to NonAux
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, auxToNonAuxOutputAddress[1]);

        // Walker
        cmdFound = expectCommand<DefaultWalkerType>(++cmdFound, cmdListQueue.end());

        // NonAux to Aux
        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[0]);

        cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(++cmdFound, cmdListQueue.end());
        verifySemaphore<FamilyType>(cmdFound, nonAuxToAuxOutputAddress[1]);
    }

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenGpuHangOnFlushBcsTaskAndBlitTranslationWhenConstructingBlockedCommandBufferAndRunningItThenEventExecutionIsAborted) {
    auto buffer0 = createBuffer(1, true);
    auto buffer1 = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 2>{{buffer0.get(), buffer1.get()}});

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);
    ultBcsCsr->callBaseFlushBcsTask = false;
    ultBcsCsr->flushBcsTaskReturnValue = CompletionStamp::gpuHang;

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};
    cl_event kernelEvent{};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, &kernelEvent);
    userEvent.setStatus(CL_COMPLETE);

    auto abortedEvent = castToObjectOrAbort<Event>(kernelEvent);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, abortedEvent->peekExecutionStatus());

    abortedEvent->release();
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationWhenEnqueueIsCalledThenDoImplicitFlushOnGpgpuCsr) {
    auto buffer = createBuffer(1, true);
    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);

    EXPECT_EQ(0u, ultCsr->taskCount);

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, ultCsr->taskCount);
    EXPECT_TRUE(ultCsr->recordedDispatchFlags.implicitFlush);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenBlitTranslationOnGfxAllocationWhenEnqueueIsCalledThenDoImplicitFlushOnGpgpuCsr) {
    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation}});

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);

    EXPECT_EQ(0u, ultCsr->taskCount);

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, ultCsr->taskCount);
    EXPECT_TRUE(ultCsr->recordedDispatchFlags.implicitFlush);

    device->getMemoryManager()->freeGraphicsMemory(gfxAllocation);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenCacheFlushRequiredWhenHandlingDependenciesThenPutAllNodesToDeferredList) {
    debugManager.flags.ForceCacheFlushForBcs.set(1);

    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation}});

    TimestampPacketContainer *deferredTimestampPackets = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->deferredTimestampPackets.get();

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(4u, deferredTimestampPackets->peekNodes().size()); // Barrier, CacheFlush, AuxToNonAux, NonAuxToAux

    device->getMemoryManager()->freeGraphicsMemory(gfxAllocation);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenCacheFlushRequiredWhenHandlingDependenciesForBlockedEnqueueThenPutAllNodesToDeferredList) {
    debugManager.flags.ForceCacheFlushForBcs.set(1);

    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation}});

    TimestampPacketContainer *deferredTimestampPackets = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->deferredTimestampPackets.get();

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    *ultCsr->getTagAddress() = 0;

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());

    userEvent.setStatus(CL_COMPLETE);

    EXPECT_FALSE(commandQueue->isQueueBlocked());

    EXPECT_EQ(4u, deferredTimestampPackets->peekNodes().size()); // Barrier, CacheFlush, AuxToNonAux, NonAuxToAux

    *ultCsr->getTagAddress() = ultCsr->taskCount;

    device->getMemoryManager()->freeGraphicsMemory(gfxAllocation);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenTerminatedLatestEnqueuedTaskWhenHandlingDependenciesForBlockedEnqueueThenDoNotPutAllNodesToDeferredListAndSetTimestampData) {
    debugManager.flags.ForceCacheFlushForBcs.set(1);

    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation}});

    TimestampPacketContainer *deferredTimestampPackets = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->timestampPacketContainer.get();

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());

    userEvent.setStatus(-1);

    EXPECT_FALSE(commandQueue->isQueueBlocked());

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(timestampPacketContainer->peekNodes()[0]->getContextEndValue(0u), 0xffffffff);

    device->getMemoryManager()->freeGraphicsMemory(gfxAllocation);
}

HWTEST_TEMPLATED_F(BlitAuxTranslationTests, givenTerminatedTaskWhenHandlingDependenciesForBlockedEnqueueThenDoNotPutAllNodesToDeferredListAndDoNotSetTimestampData) {
    debugManager.flags.ForceCacheFlushForBcs.set(1);

    auto gfxAllocation = createGfxAllocation(1, true);
    setMockKernelArgs(std::array<GraphicsAllocation *, 1>{{gfxAllocation}});

    TimestampPacketContainer *deferredTimestampPackets = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->deferredTimestampPackets.get();
    TimestampPacketContainer *timestampPacketContainer = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get())->timestampPacketContainer.get();

    UserEvent userEvent;
    [[maybe_unused]] UserEvent *ue = &userEvent;
    cl_event waitlist[] = {&userEvent};
    UserEvent userEvent1;
    [[maybe_unused]] UserEvent *ue1 = &userEvent1;
    cl_event waitlist1[] = {&userEvent1};

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist, nullptr);
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 1, waitlist1, nullptr);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());

    userEvent.setStatus(-1);

    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());
    EXPECT_EQ(timestampPacketContainer->peekNodes()[0]->getContextEndValue(0u), 1u);

    userEvent1.setStatus(-1);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
    EXPECT_EQ(1u, timestampPacketContainer->peekNodes().size());
    EXPECT_EQ(timestampPacketContainer->peekNodes()[0]->getContextEndValue(0u), 0xffffffff);

    device->getMemoryManager()->freeGraphicsMemory(gfxAllocation);
}

using BlitEnqueueWithNoTimestampPacketTests = BlitEnqueueTests<0>;

HWTEST_TEMPLATED_F(BlitEnqueueWithNoTimestampPacketTests, givenNoTimestampPacketsWritewhenEnqueueingBlitOperationThenEnginesAreSynchronized) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    const size_t bufferSize = 1u;
    auto buffer = createBuffer(bufferSize, false);
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    ASSERT_EQ(0u, ultCsr->taskCount);

    setMockKernelArgs(std::array<Buffer *, 1>{{buffer.get()}});
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    char cpuBuffer[bufferSize]{};
    commandQueue->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, bufferSize, cpuBuffer, nullptr, 0, nullptr, nullptr);
    commandQueue->finish(false);

    auto bcsCommands = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto ccsCommands = getCmdList<FamilyType>(commandQueue->getCS(0), 0);

    auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(bcsCommands.begin(), bcsCommands.end());

    cmdFound = expectMiFlush<MI_FLUSH_DW>(cmdFound++, bcsCommands.end());

    cmdFound = expectCommand<DefaultWalkerType>(ccsCommands.begin(), ccsCommands.end());

    expectNoCommand<MI_SEMAPHORE_WAIT>(cmdFound++, ccsCommands.end());
}

struct BlitEnqueueWithDebugCapabilityTests : public BlitEnqueueTests<0> {
    template <typename FamilyType>
    void findSemaphores(GenCmdList &cmdList) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        while (semaphore != cmdList.end()) {
            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
            if (static_cast<uint32_t>(DebugPauseState::hasUserStartConfirmation) == semaphoreCmd->getSemaphoreDataDword() &&
                debugPauseStateAddress == semaphoreCmd->getSemaphoreGraphicsAddress()) {

                EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());
                EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreCmd->getWaitMode());

                semaphoreBeforeCopyFound++;
            }

            if (static_cast<uint32_t>(DebugPauseState::hasUserEndConfirmation) == semaphoreCmd->getSemaphoreDataDword() &&
                debugPauseStateAddress == semaphoreCmd->getSemaphoreGraphicsAddress()) {

                EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());
                EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreCmd->getWaitMode());

                semaphoreAfterCopyFound++;
            }

            semaphore = find<MI_SEMAPHORE_WAIT *>(++semaphore, cmdList.end());
        }
    }

    template <typename FamilyType>
    void findMiFlushes(GenCmdList &cmdList) {
        using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
        auto miFlush = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

        while (miFlush != cmdList.end()) {
            auto miFlushCmd = genCmdCast<MI_FLUSH_DW *>(*miFlush);
            if (static_cast<uint32_t>(DebugPauseState::waitingForUserStartConfirmation) == miFlushCmd->getImmediateData() &&
                debugPauseStateAddress == miFlushCmd->getDestinationAddress()) {

                EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushCmd->getPostSyncOperation());

                miFlushBeforeCopyFound++;
            }

            if (static_cast<uint32_t>(DebugPauseState::waitingForUserEndConfirmation) == miFlushCmd->getImmediateData() &&
                debugPauseStateAddress == miFlushCmd->getDestinationAddress()) {

                EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushCmd->getPostSyncOperation());

                miFlushAfterCopyFound++;
            }

            miFlush = find<MI_FLUSH_DW *>(++miFlush, cmdList.end());
        }
    }

    uint32_t semaphoreBeforeCopyFound = 0;
    uint32_t semaphoreAfterCopyFound = 0;
    uint32_t miFlushBeforeCopyFound = 0;
    uint32_t miFlushAfterCopyFound = 0;

    ReleaseableObjectPtr<Buffer> buffer;
    uint64_t debugPauseStateAddress = 0;
    int hostPtr = 0;
};

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenDebugFlagSetWhenDispatchingBlitEnqueueThenAddPausingCommands) {
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    debugManager.flags.PauseOnBlitCopy.set(1);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeCopyFound);
    EXPECT_EQ(1u, semaphoreAfterCopyFound);

    findMiFlushes<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, miFlushBeforeCopyFound);
    EXPECT_EQ(1u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenGpuHangOnFlushBcsTaskAndDebugFlagSetWhenDispatchingBlitEnqueueThenOutOfResourcesIsReturned) {
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);
    ultBcsCsr->callBaseFlushBcsTask = false;
    ultBcsCsr->flushBcsTaskReturnValue = CompletionStamp::gpuHang;

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    debugManager.flags.PauseOnBlitCopy.set(1);

    const auto result = commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, result);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenDebugFlagSetToMinusTwoWhenDispatchingBlitEnqueueThenAddPausingCommandsForEachEnqueue) {
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    debugManager.flags.PauseOnBlitCopy.set(-2);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(2u, semaphoreBeforeCopyFound);
    EXPECT_EQ(2u, semaphoreAfterCopyFound);

    findMiFlushes<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(2u, miFlushBeforeCopyFound);
    EXPECT_EQ(2u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenPauseModeSetToBeforeOnlyWhenDispatchingBlitEnqueueThenAddPauseCommandsOnlyBeforeEnqueue) {
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    debugManager.flags.PauseOnBlitCopy.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeWorkload);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeCopyFound);
    EXPECT_EQ(0u, semaphoreAfterCopyFound);

    findMiFlushes<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, miFlushBeforeCopyFound);
    EXPECT_EQ(0u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenPauseModeSetToAfterOnlyWhenDispatchingBlitEnqueueThenAddPauseCommandsOnlyAfterEnqueue) {
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    debugManager.flags.PauseOnBlitCopy.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::AfterWorkload);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(0u, semaphoreBeforeCopyFound);
    EXPECT_EQ(1u, semaphoreAfterCopyFound);

    findMiFlushes<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(0u, miFlushBeforeCopyFound);
    EXPECT_EQ(1u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenPauseModeSetToBeforeAndAfterWorkloadWhenDispatchingBlitEnqueueThenAddPauseCommandsAroundEnqueue) {
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    debugPauseStateAddress = ultBcsCsr->getDebugPauseStateGPUAddress();

    buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;

    debugManager.flags.PauseOnBlitCopy.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeAndAfterWorkload);

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(ultBcsCsr->commandStream);

    findSemaphores<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, semaphoreBeforeCopyFound);
    EXPECT_EQ(1u, semaphoreAfterCopyFound);

    findMiFlushes<FamilyType>(hwParser.cmdList);

    EXPECT_EQ(1u, miFlushBeforeCopyFound);
    EXPECT_EQ(1u, miFlushAfterCopyFound);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDebugCapabilityTests, givenDebugFlagSetWhenCreatingCsrThenCreateDebugThread) {
    debugManager.flags.PauseOnBlitCopy.set(1);

    auto localDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(localDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_NE(nullptr, ultCsr->userPauseConfirmation.get());
}

struct BlitEnqueueFlushTests : public BlitEnqueueTests<1> {
    template <typename FamilyType>
    class MyUltCsr : public UltCommandStreamReceiver<FamilyType> {
      public:
        using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;

        SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            if (flushCounter) {
                latestFlushedCounter = ++(*flushCounter);
            }
            return UltCommandStreamReceiver<FamilyType>::flush(batchBuffer, allocationsForResidency);
        }

        static CommandStreamReceiver *create(bool withAubDump, ExecutionEnvironment &executionEnvironment,
                                             uint32_t rootDeviceIndex,
                                             const DeviceBitfield deviceBitfield) {
            return new MyUltCsr<FamilyType>(executionEnvironment, rootDeviceIndex, deviceBitfield);
        }

        uint32_t *flushCounter = nullptr;
        uint32_t latestFlushedCounter = 0;
    };

    template <typename T>
    void setUpT() {
        auto csrCreateFcn = &commandStreamReceiverFactory[IGFX_MAX_CORE + defaultHwInfo->platform.eRenderCoreFamily];
        variableBackup = std::make_unique<VariableBackup<CommandStreamReceiverCreateFunc>>(csrCreateFcn);
        *csrCreateFcn = MyUltCsr<T>::create;

        BlitEnqueueTests<1>::setUpT<T>();
    }

    std::unique_ptr<VariableBackup<CommandStreamReceiverCreateFunc>> variableBackup;
};

HWTEST_TEMPLATED_F(BlitEnqueueFlushTests, givenNonBlockedQueueWhenBlitEnqueuedThenFlushGpgpuCsrFirst) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    uint32_t flushCounter = 0;

    auto myUltGpgpuCsr = static_cast<MyUltCsr<FamilyType> *>(gpgpuCsr);
    myUltGpgpuCsr->flushCounter = &flushCounter;
    auto myUltBcsCsr = static_cast<MyUltCsr<FamilyType> *>(bcsCsr);
    myUltBcsCsr->flushCounter = &flushCounter;

    commandQueue->enqueueWriteBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, myUltGpgpuCsr->latestFlushedCounter);
    EXPECT_EQ(2u, myUltBcsCsr->latestFlushedCounter);
}

HWTEST_TEMPLATED_F(BlitEnqueueFlushTests, givenGpuHangAndBlockingCallAndNonBlockedQueueWhenBlitEnqueuedThenOutOfResourcesIsReturned) {
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    uint32_t flushCounter = 0;

    auto myUltGpgpuCsr = static_cast<MyUltCsr<FamilyType> *>(gpgpuCsr);
    myUltGpgpuCsr->flushCounter = &flushCounter;
    auto myUltBcsCsr = static_cast<MyUltCsr<FamilyType> *>(bcsCsr);
    myUltBcsCsr->flushCounter = &flushCounter;

    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCommandQueue->waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const auto enqueueResult = mockCommandQueue->enqueueWriteBuffer(buffer.get(), CL_FALSE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueue->waitForAllEnginesCalledCount);
}

HWTEST_TEMPLATED_F(BlitEnqueueFlushTests, givenBlockedQueueWhenBlitEnqueuedThenFlushGpgpuCsrFirst) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    uint32_t flushCounter = 0;

    auto myUltGpgpuCsr = static_cast<MyUltCsr<FamilyType> *>(gpgpuCsr);
    myUltGpgpuCsr->flushCounter = &flushCounter;
    auto myUltBcsCsr = static_cast<MyUltCsr<FamilyType> *>(bcsCsr);
    myUltBcsCsr->flushCounter = &flushCounter;

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, waitlist, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(1u, myUltGpgpuCsr->latestFlushedCounter);
    EXPECT_EQ(2u, myUltBcsCsr->latestFlushedCounter);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueFlushTests, givenGpuHangOnFlushBcsTaskAndBlockedQueueWhenBlitEnqueuedThenEventIsAborted) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    uint32_t flushCounter = 0;

    auto myUltGpgpuCsr = static_cast<MyUltCsr<FamilyType> *>(gpgpuCsr);
    myUltGpgpuCsr->flushCounter = &flushCounter;

    auto myUltBcsCsr = static_cast<MyUltCsr<FamilyType> *>(bcsCsr);
    myUltBcsCsr->flushCounter = &flushCounter;
    myUltBcsCsr->callBaseFlushBcsTask = false;
    myUltBcsCsr->flushBcsTaskReturnValue = CompletionStamp::gpuHang;

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};
    cl_event writeBufferEvent{};

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, waitlist, &writeBufferEvent);
    userEvent.setStatus(CL_COMPLETE);

    auto abortedEvent = castToObjectOrAbort<Event>(writeBufferEvent);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, abortedEvent->peekExecutionStatus());

    abortedEvent->release();
}

using BlitEnqueueTaskCountTests = BlitEnqueueTests<1>;

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, whenWaitUntilCompletionCalledThenWaitForSpecificBcsTaskCount) {
    uint32_t gpgpuTaskCount = 123;
    uint32_t bcsTaskCount = 123;

    CopyEngineState bcsState{bcsCsr->getOsContext().getEngineType(), bcsTaskCount};
    commandQueue->waitUntilComplete(gpgpuTaskCount, std::span{&bcsState, 1}, 0, false);

    EXPECT_EQ(gpgpuTaskCount, static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(bcsTaskCount, static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenEventWithNotreadyBcsTaskCountThenDontReportCompletion) {
    const uint32_t gpgpuTaskCount = 123;
    const uint32_t bcsTaskCount = 123;

    *gpgpuCsr->getTagAddress() = gpgpuTaskCount;
    *bcsCsr->getTagAddress() = bcsTaskCount - 1;
    commandQueue->updateBcsTaskCount(bcsCsr->getOsContext().getEngineType(), bcsTaskCount);

    Event event(commandQueue.get(), CL_COMMAND_WRITE_BUFFER, 1, gpgpuTaskCount);
    event.setupBcs(bcsCsr->getOsContext().getEngineType());
    event.updateCompletionStamp(gpgpuTaskCount, bcsTaskCount, 1, 0);

    event.updateExecutionStatus();
    EXPECT_EQ(static_cast<cl_int>(CL_SUBMITTED), event.peekExecutionStatus());

    *bcsCsr->getTagAddress() = bcsTaskCount;
    event.updateExecutionStatus();
    EXPECT_EQ(static_cast<cl_int>(CL_COMPLETE), event.peekExecutionStatus());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenEventWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    cl_event outEvent1, outEvent2;
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent1);
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent2);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(2u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(2u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(1u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(1u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenBufferDumpingEnabledWhenEnqueueingThenSetCorrectDumpOption) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    debugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);
    debugManager.flags.AUBDumpBufferFormat.set("BIN");

    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());

    {
        // BCS enqueue
        commandQueue->enqueueReadBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

        EXPECT_TRUE(mockCommandQueue->notifyEnqueueReadBufferCalled);
        EXPECT_TRUE(mockCommandQueue->useBcsCsrOnNotifyEnabled);

        mockCommandQueue->notifyEnqueueReadBufferCalled = false;
    }

    {
        // Non-BCS enqueue
        debugManager.flags.EnableBlitterForEnqueueOperations.set(0);

        commandQueue->enqueueReadBuffer(buffer.get(), true, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

        EXPECT_TRUE(mockCommandQueue->notifyEnqueueReadBufferCalled);
        EXPECT_FALSE(mockCommandQueue->useBcsCsrOnNotifyEnabled);
    }
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenBlockedEventWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    cl_event outEvent1, outEvent2;
    UserEvent userEvent;
    cl_event waitlist1 = &userEvent;
    cl_event *waitlist2 = &outEvent1;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, &waitlist1, &outEvent1);
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, waitlist2, &outEvent2);

    userEvent.setStatus(CL_COMPLETE);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(3u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(2u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(1u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(1u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenBlockedEnqueueWithoutKernelWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    cl_event outEvent1, outEvent2;
    UserEvent userEvent;
    cl_event waitlist1 = &userEvent;
    cl_event *waitlist2 = &outEvent1;

    commandQueue->enqueueMarkerWithWaitList(1, &waitlist1, &outEvent1);
    commandQueue->enqueueMarkerWithWaitList(1, waitlist2, &outEvent2);

    userEvent.setStatus(CL_COMPLETE);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(2u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(0u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(1u, ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(0u, ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenEventFromCpuCopyWhenWaitingForCompletionThenWaitForCurrentBcsTaskCount) {
    auto buffer = createBuffer(1, false);
    int hostPtr = 0;

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    ultGpgpuCsr->taskCount = 1;
    commandQueue->taskCount = 1;

    ultBcsCsr->taskCount = 2;
    commandQueue->updateBcsTaskCount(ultBcsCsr->getOsContext().getEngineType(), 2);

    cl_event outEvent1, outEvent2;
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent1);
    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &outEvent2);

    clWaitForEvents(1, &outEvent2);
    EXPECT_EQ(3u, static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(4u, static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());

    clWaitForEvents(1, &outEvent1);
    EXPECT_EQ(2u, static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());
    EXPECT_EQ(3u, static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr)->latestWaitForCompletionWithTimeoutTaskCount.load());

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenMarkerThatFollowsCopyOperationWhenItIsWaitedItHasProperDependencies) {
    auto buffer = createBuffer(1, false);
    int hostPtr = 0;

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto mockCmdQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCmdQueue->commandQueueProperties |= CL_QUEUE_PROFILING_ENABLE;

    cl_event outEvent1;
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    auto offset = mockCmdQueue->getCS(0).getUsed();

    // marker needs to program semaphore
    commandQueue->enqueueMarkerWithWaitList(0, nullptr, &outEvent1);

    auto cmdListQueue = getCmdList<FamilyType>(mockCmdQueue->getCS(0), offset);
    expectCommand<MI_SEMAPHORE_WAIT>(cmdListQueue.begin(), cmdListQueue.end());

    clReleaseEvent(outEvent1);
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenWaitlistWithTimestampPacketWhenEnqueueingThenDeferWaitlistNodes) {
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    auto buffer = createBuffer(1, false);

    auto mockCmdQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());

    TimestampPacketContainer *deferredTimestampPackets = mockCmdQueue->deferredTimestampPackets.get();

    MockTimestampPacketContainer timestamp(*device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event waitlistEvent(mockCmdQueue, 0, 0, 0);
    waitlistEvent.addTimestampPacketNodes(timestamp);

    cl_event waitlist[] = {&waitlistEvent};

    mockCmdQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, waitlist, nullptr);

    auto deferredNodesCount = deferredTimestampPackets->peekNodes().size();

    EXPECT_TRUE(deferredNodesCount >= 1);

    bool waitlistNodeFound = false;
    for (auto &node : deferredTimestampPackets->peekNodes()) {
        if (node->getGpuAddress() == timestamp.peekNodes()[0]->getGpuAddress()) {
            waitlistNodeFound = true;
        }
    }

    EXPECT_TRUE(waitlistNodeFound);

    mockCmdQueue->flush();
    EXPECT_EQ(deferredNodesCount, deferredTimestampPackets->peekNodes().size());

    mockCmdQueue->finish(false);
    EXPECT_EQ(0u, deferredTimestampPackets->peekNodes().size());
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenMarkerThatFollowsCopyOperationWhenItIsWaitedItHasProperDependenciesOnWait) {
    auto buffer = createBuffer(1, false);
    int hostPtr = 0;

    cl_event outEvent1;
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    commandQueue->enqueueMarkerWithWaitList(0, nullptr, &outEvent1);

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    // make sure we wait for both
    clWaitForEvents(1, &outEvent1);
    EXPECT_NE(ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount, ultBcsCsr->taskCount);
    EXPECT_EQ(ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount, ultGpgpuCsr->taskCount);

    clWaitForEvents(1, &outEvent1);

    clReleaseEvent(outEvent1);
}

HWTEST_TEMPLATED_F(BlitEnqueueTaskCountTests, givenMarkerThatFollowsCopyOperationWhenItIsWaitedItHasProperDependenciesOnWaitEvenWhenMultipleMarkersAreSequenced) {
    auto buffer = createBuffer(1, false);
    int hostPtr = 0;

    cl_event outEvent1, outEvent2;
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);

    commandQueue->enqueueMarkerWithWaitList(0, nullptr, &outEvent1);
    commandQueue->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    commandQueue->enqueueMarkerWithWaitList(0, nullptr, &outEvent2);

    auto ultGpgpuCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(gpgpuCsr);
    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsCsr);

    // make sure we wait for both
    clWaitForEvents(1, &outEvent2);
    EXPECT_NE(ultBcsCsr->latestWaitForCompletionWithTimeoutTaskCount, ultBcsCsr->taskCount);
    EXPECT_EQ(ultGpgpuCsr->latestWaitForCompletionWithTimeoutTaskCount, ultGpgpuCsr->taskCount);

    clWaitForEvents(1, &outEvent2);

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);
}

} // namespace NEO
