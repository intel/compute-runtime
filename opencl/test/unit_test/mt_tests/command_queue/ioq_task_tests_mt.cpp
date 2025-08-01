/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include <future>

using namespace NEO;

struct IOQTaskTestsMt : public HelloWorldTest<HelloWorldFixtureFactory> {
    void SetUp() override {
        ClDeviceFixture::setUp();
        ASSERT_NE(nullptr, pClDevice);
        pDevice->disableSecondaryEngines = true;

        CommandQueueFixture::setUp(pClDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
        CommandStreamFixture::setUp(pCmdQ);
        ASSERT_NE(nullptr, pCS);
        IndirectHeapFixture::setUp(pCmdQ);
        KernelFixture::setUp(pClDevice);
        ASSERT_NE(nullptr, pKernel);

        auto retVal = CL_INVALID_VALUE;
        BufferDefaults::context = new MockContext(pClDevice);

        destBuffer = Buffer::create(
            BufferDefaults::context,
            CL_MEM_READ_WRITE,
            sizeUserMemory,
            nullptr,
            retVal);

        srcBuffer = Buffer::create(
            BufferDefaults::context,
            CL_MEM_READ_WRITE,
            sizeUserMemory,
            nullptr,
            retVal);

        pDestMemory = destBuffer->getCpuAddressForMapping();
        pSrcMemory = srcBuffer->getCpuAddressForMapping();

        memset(pDestMemory, destPattern, sizeUserMemory);
        memset(pSrcMemory, srcPattern, sizeUserMemory);

        pKernel->setArg(0, srcBuffer);
        pKernel->setArg(1, destBuffer);
    }
};

TEST_F(IOQTaskTestsMt, GivenBlockingAndBlockedOnUserEventWhenReadingBufferThenTaskCountAndTaskLevelAreIncremented) {
    USE_REAL_FILE_SYSTEM();
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto alignedReadPtr = alignedMalloc(BufferDefaults::sizeInBytes, MemoryConstants::cacheLineSize);
    ASSERT_NE(nullptr, alignedReadPtr);

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto previousTaskLevel = pCmdQ->taskLevel;
    auto previousTaskCount = pCmdQ->taskCount;

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
    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);

    t.join();

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    alignedFree(alignedReadPtr);
}

TEST_F(IOQTaskTestsMt, GivenBlockedOnUserEventWhenEnqueingMarkerThenSuccessIsReturned) {

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

TEST_F(IOQTaskTestsMt, GivenMultipleThreadsWhenMappingBufferThenEventsAreCompleted) {
    MockGraphicsAllocation alignedBufferAlloc{nullptr, MemoryConstants::pageSize};
    AlignedBuffer alignedBuffer{pContext, &alignedBufferAlloc};

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_event outputEvent = nullptr;
    void *mappedPtr = pCmdQ->enqueueMapBuffer(&alignedBuffer, false, CL_MAP_READ, 0, alignedBuffer.getSize(), 1, &userEvent, &outputEvent, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);

    const int32_t numThreads = 20;

    std::thread threads[numThreads];
    std::thread threadUnblocking;

    cl_event ouputEventsFromThreads[numThreads];
    void *mappedPtrs[numThreads];

    for (int32_t i = 0; i < numThreads; i++) {
        threads[i] = std::thread([&](int32_t index) {
            cl_int errCode = CL_SUCCESS;
            cl_int success = CL_SUCCESS;
            mappedPtrs[index] = pCmdQ->enqueueMapBuffer(&alignedBuffer, false, CL_MAP_READ, 0, alignedBuffer.getSize(), 0, nullptr, &ouputEventsFromThreads[index], errCode);
            EXPECT_EQ(success, errCode);
        },
                                 i);

        if (i == numThreads / 2) {
            threadUnblocking = std::thread([=]() {
                auto ret = clSetUserEventStatus(userEvent, CL_COMPLETE);
                EXPECT_EQ(CL_SUCCESS, ret);
            });
        }
    }

    cl_int errCode = clWaitForEvents(1, &outputEvent);
    EXPECT_EQ(CL_SUCCESS, errCode);

    cl_int eventStatus = 0;
    errCode = clGetEventInfo(outputEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(CL_COMPLETE, eventStatus);

    for (int32_t i = 0; i < numThreads; i++) {
        threads[i].join();
        cl_int errCode = clWaitForEvents(1, &ouputEventsFromThreads[i]);
        EXPECT_EQ(CL_SUCCESS, errCode);
        errCode = clGetEventInfo(outputEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, nullptr);
        EXPECT_EQ(CL_SUCCESS, errCode);
        EXPECT_EQ(CL_COMPLETE, eventStatus);
    }

    threadUnblocking.join();
    retVal = clReleaseEvent(userEvent);

    for (int32_t i = 0; i < numThreads; i++) {
        pCmdQ->enqueueUnmapMemObject(&alignedBuffer, mappedPtrs[i], 0, nullptr, nullptr);
        retVal = clReleaseEvent(ouputEventsFromThreads[i]);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    pCmdQ->enqueueUnmapMemObject(&alignedBuffer, mappedPtr, 0, nullptr, nullptr);

    retVal = clReleaseEvent(outputEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(IOQTaskTestsMt, GivenMultipleThreadsWhenMappingImageThenEventsAreCompleted) {
    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image1dDefaults>::create(context));

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_event outputEvent = nullptr;
    const size_t origin[] = {0, 0, 0};
    const size_t region[] = {1, 1, 1};

    void *mappedPtr = pCmdQ->enqueueMapImage(image.get(), false, CL_MAP_READ, origin, region, nullptr, nullptr, 1, &userEvent, &outputEvent, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);

    const int32_t numThreads = 20;

    std::thread threads[numThreads];
    std::thread threadUnblocking;

    cl_event ouputEventsFromThreads[numThreads];
    void *mappedPtrs[numThreads];

    for (int32_t i = 0; i < numThreads; i++) {
        threads[i] = std::thread([&](int32_t index) {
            cl_int errCode = CL_SUCCESS;
            cl_int success = CL_SUCCESS;
            mappedPtrs[index] = pCmdQ->enqueueMapImage(image.get(), false, CL_MAP_READ, origin, region, nullptr, nullptr, 0, nullptr, &ouputEventsFromThreads[index], errCode);
            EXPECT_EQ(success, errCode);
        },
                                 i);

        if (i == numThreads / 2) {
            threadUnblocking = std::thread([=]() {
                auto ret = clSetUserEventStatus(userEvent, CL_COMPLETE);
                EXPECT_EQ(CL_SUCCESS, ret);
            });
        }
    }

    cl_int errCode = clWaitForEvents(1, &outputEvent);
    EXPECT_EQ(CL_SUCCESS, errCode);

    cl_int eventStatus = 0;
    errCode = clGetEventInfo(outputEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(CL_COMPLETE, eventStatus);

    for (int32_t i = 0; i < numThreads; i++) {
        threads[i].join();
        cl_int errCode = clWaitForEvents(1, &ouputEventsFromThreads[i]);
        EXPECT_EQ(CL_SUCCESS, errCode);
        errCode = clGetEventInfo(outputEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, nullptr);
        EXPECT_EQ(CL_SUCCESS, errCode);
        EXPECT_EQ(CL_COMPLETE, eventStatus);
    }

    threadUnblocking.join();
    retVal = clReleaseEvent(userEvent);

    for (int32_t i = 0; i < numThreads; i++) {
        pCmdQ->enqueueUnmapMemObject(image.get(), mappedPtrs[i], 0, nullptr, nullptr);
        retVal = clReleaseEvent(ouputEventsFromThreads[i]);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    pCmdQ->enqueueUnmapMemObject(image.get(), mappedPtr, 0, nullptr, nullptr);

    retVal = clReleaseEvent(outputEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(IOQTaskTestsMt, givenBlitterWhenCopyUsingMultipleThreadsThenSuccessReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);

    constexpr uint32_t numThreads = 32;
    std::atomic_uint32_t barrier = numThreads;
    std::array<std::future<void>, numThreads> threads;

    auto device = MockClDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, rootDeviceIndex);
    REQUIRE_FULL_BLITTER_OR_SKIP(device->getRootDeviceEnvironment());
    MockClDevice clDevice(device);
    auto cmdQ = createCommandQueue(&clDevice);
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
            auto retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(cmdQ,
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

    EXPECT_EQ(cmdQ->taskCount, 0u);
    EXPECT_EQ(cmdQ->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQ->peekBcsTaskCount(aub_stream::EngineType::ENGINE_BCS), numThreads);

    clReleaseCommandQueue(cmdQ);
}