/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

#include <future>

using namespace NEO;

struct OOQFixtureFactory : public HelloWorldFixtureFactory {
    typedef OOQueueFixture CommandQueueFixture;
};

template <typename TypeParam>
struct OOQTaskTypedTestsMt : public HelloWorldTest<OOQFixtureFactory> {
};

typedef OOQTaskTypedTestsMt<EnqueueKernelHelper<>> OOQTaskTestsMt;

TEST_F(OOQTaskTestsMt, GivenBlockingAndBlockedOnUserEventWhenReadingBufferThenTaskCountIsIncrementedAndTaskLevelIsUnchanged) {
    USE_REAL_FILE_SYSTEM();
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto alignedReadPtr = alignedMalloc(BufferDefaults::sizeInBytes, MemoryConstants::cacheLineSize);
    ASSERT_NE(nullptr, alignedReadPtr);

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto previousTaskCount = pCmdQ->taskCount;
    auto previousTaskLevel = pCmdQ->taskLevel;

    std::thread t([=]() {
        Event *ev = castToObject<Event>(userEvent);
        while (ev->peekHasChildEvents() == false) {
            // active wait for VirtualEvent (which is added after queue is blocked)
        }
        auto ret = clSetUserEventStatus(userEvent, CL_COMPLETE);
        ASSERT_EQ(CL_SUCCESS, ret);
    });

    buffer->forceDisallowCPUCopy = true; // no task level incrasing when cpu copy
    retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ,
                                                          buffer.get(),
                                                          CL_TRUE,
                                                          0,
                                                          BufferDefaults::sizeInBytes,
                                                          alignedReadPtr,
                                                          nullptr,
                                                          1,
                                                          &userEvent,
                                                          nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_LT(previousTaskCount, pCmdQ->taskCount);
    EXPECT_EQ(previousTaskLevel, pCmdQ->taskLevel);

    t.join();

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    alignedFree(alignedReadPtr);
}

TEST_F(OOQTaskTestsMt, GivenBlockedOnUserEventWhenEnqueingMarkerThenSuccessIsReturned) {

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    std::thread t([=]() {
        Event *ev = castToObject<Event>(userEvent);
        while (ev->peekHasChildEvents() == false) {
            // active wait for VirtualEvent (which is added after queue is blocked)
        }
        auto ret = clSetUserEventStatus(userEvent, CL_COMPLETE);
        ASSERT_EQ(CL_SUCCESS, ret);
    });

    retVal = pCmdQ->enqueueMarkerWithWaitList(
        1,
        &userEvent,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    t.join();

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(OOQTaskTestsMt, givenBlitterWhenEnqueueCopyAndKernelUsingMultipleThreadsThenSuccessReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(0);

    constexpr uint32_t numThreads = 32;
    std::atomic_uint32_t barrier = numThreads;
    std::array<std::future<void>, numThreads> threads;

    auto device = MockClDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, rootDeviceIndex);
    REQUIRE_FULL_BLITTER_OR_SKIP(device->getRootDeviceEnvironment());

    MockClDevice clDevice(device);
    auto cmdQ = createCommandQueue(&clDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    EXPECT_EQ(cmdQ->taskCount, 0u);
    EXPECT_EQ(cmdQ->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQ->peekBcsTaskCount(aub_stream::EngineType::ENGINE_BCS), 0u);
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    for (auto &thread : threads) {
        thread = std::async(std::launch::async, [&]() {
            auto alignedReadPtr = alignedMalloc(BufferDefaults::sizeInBytes, MemoryConstants::cacheLineSize);
            barrier.fetch_sub(1u);
            while (barrier.load() != 0u) {
                std::this_thread::yield();
            }

            auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(cmdQ,
                                                                         buffer.get(),
                                                                         CL_TRUE,
                                                                         0,
                                                                         BufferDefaults::sizeInBytes,
                                                                         alignedReadPtr,
                                                                         nullptr,
                                                                         0,
                                                                         nullptr,
                                                                         nullptr);
            EXPECT_EQ(CL_SUCCESS, retVal);

            size_t workSize[] = {64};
            retVal = EnqueueKernelHelper<>::enqueueKernel(cmdQ, KernelFixture::pKernel, 1, nullptr, workSize, workSize, 0, nullptr, nullptr);
            EXPECT_EQ(CL_SUCCESS, retVal);

            retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(cmdQ,
                                                                  buffer.get(),
                                                                  CL_TRUE,
                                                                  0,
                                                                  BufferDefaults::sizeInBytes,
                                                                  alignedReadPtr,
                                                                  nullptr,
                                                                  0,
                                                                  nullptr,
                                                                  nullptr);
            EXPECT_EQ(CL_SUCCESS, retVal);

            alignedFree(alignedReadPtr);
        });
    }
    for (auto &thread : threads) {
        thread.get();
    }

    EXPECT_NE(cmdQ->taskCount, 0u);
    EXPECT_NE(cmdQ->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQ->peekBcsTaskCount(aub_stream::EngineType::ENGINE_BCS), 2 * numThreads);

    clReleaseCommandQueue(cmdQ);
}