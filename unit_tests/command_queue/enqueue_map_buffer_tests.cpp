/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_queue/enqueue_map_buffer_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"

using namespace NEO;

struct EnqueueMapBufferTest : public DeviceFixture,
                              public CommandQueueHwFixture,
                              public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueMapBufferTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pDevice, 0);
        BufferDefaults::context = new MockContext;

        buffer = BufferHelper<BufferUseHostPtr<>>::create();
    }

    void TearDown() override {
        delete buffer;
        delete BufferDefaults::context;
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    Buffer *buffer = nullptr;
    char srcMemory[128];
};

TEST_F(EnqueueMapBufferTest, GivenBufferAddressesWhenMappingBufferThenCpuAndGpuAddressAreEqualWhenZeroCopyIsUsed) {
    auto mapFlags = CL_MAP_READ;
    auto size = 0;
    auto offset = 0;
    cl_int retVal;
    auto ptr = pCmdQ->enqueueMapBuffer(
        buffer,
        true,
        mapFlags,
        offset,
        size,
        0,
        nullptr,
        nullptr,
        retVal);
    if (buffer->isMemObjZeroCopy()) {
        EXPECT_EQ(buffer->getCpuAddress(), ptr);
    } else {
        EXPECT_NE(buffer->getCpuAddress(), ptr);
    }
}

TEST_F(EnqueueMapBufferTest, givenBufferWithUseHostPtrFlagWhenMappedThenReturnHostPtr) {
    auto hostPtr = buffer->getHostPtr();
    EXPECT_NE(nullptr, hostPtr);
    auto mapFlags = CL_MAP_READ;
    auto size = 2;
    auto offset = 2;
    cl_int retVal;
    auto ptr = pCmdQ->enqueueMapBuffer(buffer, true, mapFlags, offset, size,
                                       0, nullptr, nullptr, retVal);

    EXPECT_EQ(ptr, ptrOffset(hostPtr, offset));
}

TEST_F(EnqueueMapBufferTest, GivenCmdqAndValidArgsWhenMappingBufferThenSuccessIsReturned) {
    auto mapFlags = CL_MAP_READ;
    auto size = 0;
    auto offset = 0;
    auto retVal = CL_INVALID_VALUE;
    auto ptr = pCmdQ->enqueueMapBuffer(
        buffer,
        true,
        mapFlags,
        offset,
        size,
        0,
        nullptr,
        nullptr,
        retVal);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueMapBufferTest, GivenChangesInHostBufferWhenMappingBufferThenChangesArePropagatedToDeviceMemory) {
    //size not aligned to cacheline size
    int bufferSize = 20;
    void *ptrHost = malloc(bufferSize);
    char *charHostPtr = static_cast<char *>(ptrHost);

    //first fill with data
    for (int i = 0; i < bufferSize; i++) {
        charHostPtr[i] = 1;
    }

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_USE_HOST_PTR,
        bufferSize,
        charHostPtr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_TRUE,
        CL_MAP_WRITE,
        0,
        8,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ptrResult, charHostPtr) << "Map Buffer should return host_pointer used during creation with CL_MEM_USE_HOST_PTR";

    //check data
    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(charHostPtr[i], 1);
        //change the data
        charHostPtr[i] = 2;
    }

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        buffer,
        ptrResult,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //now map again and see if data propagated
    clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_TRUE,
        CL_MAP_WRITE,
        0,
        8,
        0,
        nullptr,
        nullptr,
        &retVal);

    //check data
    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(charHostPtr[i], 2);
    }

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    free(ptrHost);
}

TEST_F(EnqueueMapBufferTest, GivenChangesInHostBufferWithOffsetWhenMappingBufferThenChangesArePropagatedToDeviceMemory) {
    //size not aligned to cacheline size
    int bufferSize = 20;
    void *ptrHost = malloc(bufferSize);
    char *charHostPtr = static_cast<char *>(ptrHost);
    size_t offset = 4;

    //first fill with data
    for (int i = 0; i < bufferSize; i++) {
        charHostPtr[i] = 1;
    }

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_USE_HOST_PTR,
        bufferSize,
        charHostPtr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_TRUE,
        CL_MAP_WRITE,
        offset,
        8,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ptrResult, charHostPtr + offset) << "Map Buffer should return host_pointer used during creation with CL_MEM_USE_HOST_PTR";

    //check data
    for (int i = (int)offset; i < (int)(bufferSize - (int)offset); i++) {
        EXPECT_EQ(charHostPtr[i], 1);
    }

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        buffer,
        ptrResult,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    free(ptrHost);
}

TEST_F(EnqueueMapBufferTest, GivenValidArgsWhenMappingBufferThenSuccessIsReturned) {
    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_TRUE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        nullptr,
        &retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueMapBufferTest, givenNonBlockingReadOnlyMapBufferOnZeroCopyBufferWhenItIsCalledThenSynchronizationIsNotMadeUntilWaitForEvents) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    cl_event mapEventReturned = nullptr;
    cl_event unmapEventReturned = nullptr;
    *pTagMemory = 0;
    MockKernelWithInternals kernel(*pDevice);
    size_t GWS = 1;

    struct E2Clb {
        static void CL_CALLBACK SignalEv2(cl_event e, cl_int status, void *data) {
            uint32_t *callbackCalled = static_cast<uint32_t *>(data);
            *callbackCalled = 1;
        }
    };

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    uint32_t taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(0u, taskCount);

    // enqueue something that can be finished...
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, 0, &GWS, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_FALSE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        &mapEventReturned,
        &retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //no dc flush required at this point
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(1u, taskCount);

    auto neoEvent = castToObject<Event>(mapEventReturned);
    //if task count of csr is higher then event task count with proper dc flushing then we are fine
    EXPECT_EQ(1u, neoEvent->getCompletionStamp());
    //this can't be completed as task count is not reached yet
    EXPECT_FALSE(neoEvent->updateStatusAndCheckCompletion());
    EXPECT_TRUE(CL_COMMAND_MAP_BUFFER == neoEvent->getCommandType());

    auto callbackCalled = 0u;

    *pTagMemory += 4;

    clSetEventCallback(mapEventReturned, CL_COMPLETE, E2Clb::SignalEv2, (void *)&callbackCalled);

    //wait for events needs to flush DC as event requires this.
    retVal = clWaitForEvents(1, &mapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //wait for event do not sent flushTask
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(1u, pCmdQ->latestTaskCountWaited);

    EXPECT_TRUE(neoEvent->updateStatusAndCheckCompletion());

    EXPECT_EQ(1u, callbackCalled);

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        buffer,
        ptrResult,
        0,
        nullptr,
        &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    auto unmapEvent = castToObject<Event>(unmapEventReturned);
    EXPECT_TRUE(CL_COMMAND_UNMAP_MEM_OBJECT == unmapEvent->getCommandType());
    retVal = clWaitForEvents(1, &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(mapEventReturned);
    clReleaseEvent(unmapEventReturned);
}

TEST_F(EnqueueMapBufferTest, givenNonReadOnlyBufferWhenMappedOnGpuThenSetValidEventCmds) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    cl_event mapEventReturned = nullptr;
    cl_event unmapEventReturned = nullptr;
    *pTagMemory = 5;

    std::unique_ptr<Buffer> buffer(Buffer::create(BufferDefaults::context, CL_MEM_READ_WRITE, 20, nullptr, retVal));
    buffer->setSharingHandler(new SharingHandler());
    buffer->forceDisallowCPUCopy = true;
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer.get());

    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    EXPECT_EQ(0u, commandStreamReceiver.peekTaskCount());

    auto ptrResult = clEnqueueMapBuffer(pCmdQ, buffer.get(), CL_FALSE, CL_MAP_WRITE, 0, 8, 0,
                                        nullptr, &mapEventReturned, &retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    auto mapEvent = castToObject<Event>(mapEventReturned);
    EXPECT_TRUE(CL_COMMAND_MAP_BUFFER == mapEvent->getCommandType());

    retVal = clWaitForEvents(1, &mapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(pCmdQ, buffer.get(), ptrResult, 0, nullptr, &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, commandStreamReceiver.peekTaskCount());

    auto unmapEvent = castToObject<Event>(unmapEventReturned);
    EXPECT_TRUE(CL_COMMAND_UNMAP_MEM_OBJECT == unmapEvent->getCommandType());

    retVal = clWaitForEvents(1, &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(mapEventReturned);
    clReleaseEvent(unmapEventReturned);
}

TEST_F(EnqueueMapBufferTest, givenReadOnlyBufferWhenMappedOnGpuThenSetValidEventCmds) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    cl_event mapEventReturned = nullptr;
    cl_event unmapEventReturned = nullptr;
    *pTagMemory = 5;

    std::unique_ptr<Buffer> buffer(Buffer::create(BufferDefaults::context, CL_MEM_READ_WRITE, 20, nullptr, retVal));
    buffer->setSharingHandler(new SharingHandler());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer.get());

    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    EXPECT_EQ(0u, commandStreamReceiver.peekTaskCount());

    auto ptrResult = clEnqueueMapBuffer(pCmdQ, buffer.get(), CL_FALSE, CL_MAP_READ, 0, 8, 0,
                                        nullptr, &mapEventReturned, &retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    auto mapEvent = castToObject<Event>(mapEventReturned);
    EXPECT_TRUE(CL_COMMAND_MAP_BUFFER == mapEvent->getCommandType());

    retVal = clWaitForEvents(1, &mapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(pCmdQ, buffer.get(), ptrResult, 0, nullptr, &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    auto unmapEvent = castToObject<Event>(unmapEventReturned);
    EXPECT_TRUE(CL_COMMAND_UNMAP_MEM_OBJECT == unmapEvent->getCommandType());

    retVal = clWaitForEvents(1, &unmapEventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(mapEventReturned);
    clReleaseEvent(unmapEventReturned);
}

TEST_F(EnqueueMapBufferTest, givenNonBlockingMapBufferAfterL3IsAlreadyFlushedThenEventIsSignaledAsCompleted) {
    cl_event eventReturned = nullptr;
    uint32_t tagHW = 0;
    *pTagMemory = tagHW;
    MockKernelWithInternals kernel(*pDevice);
    size_t GWS = 1;

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    uint32_t taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(0u, taskCount);

    // enqueue something that map buffer needs to wait for
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, 0, &GWS, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto NDRcompletionStamp = commandStreamReceiver.peekTaskCount();

    //simulate that NDR is done and DC was flushed
    auto forcedLatestSentDC = NDRcompletionStamp + 1;
    *pTagMemory = forcedLatestSentDC;

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_FALSE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        &eventReturned,
        &retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(1u, taskCount);

    auto neoEvent = castToObject<Event>(eventReturned);
    //if task count of csr is higher then event task count with proper dc flushing then we are fine
    EXPECT_EQ(1u, neoEvent->getCompletionStamp());
    EXPECT_TRUE(neoEvent->updateStatusAndCheckCompletion());

    //flush task was not called
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    //wait for events shouldn't call flush task
    retVal = clWaitForEvents(1, &eventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(eventReturned);
}

TEST_F(EnqueueMapBufferTest, GivenBufferThatIsNotZeroCopyWhenNonBlockingMapIsCalledThenFinishIsCalledAndDataTransferred) {
    const auto bufferSize = 100;
    auto localSize = bufferSize;
    char misaligned[bufferSize] = {1};
    MockKernelWithInternals kernel(*pDevice);
    size_t GWS = 1;

    uintptr_t address = (uintptr_t)&misaligned[0];

    if (!(address & (MemoryConstants::cacheLineSize - 1))) {
        address++;
        localSize--;
    }

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_USE_HOST_PTR,
        localSize,
        (void *)address,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto pBuffer = castToObject<Buffer>(buffer);
    ASSERT_FALSE(pBuffer->isMemObjZeroCopy());

    // enqueue something that can be finished
    retVal = clEnqueueNDRangeKernel(pCmdQ, kernel, 1, 0, &GWS, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    uint32_t taskCount = commandStreamReceiver.peekTaskCount();
    EXPECT_EQ(1u, taskCount);

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_FALSE,
        CL_MAP_READ,
        0,
        localSize,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    commandStreamReceiver.peekTaskCount();

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, pCmdQ->latestTaskCountWaited);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueMapBufferTest, GivenWrongMemObjectWhenMapIsCalledThenInvalidMemObjectErrorCodeIsReturned) {
    MockBuffer buffer;
    cl_mem mem = &buffer;
    buffer.magic = -1;

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        mem,
        CL_FALSE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(nullptr, ptrResult);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(EnqueueMapBufferTest, GivenPtrToReturnEventWhenMappingBufferThenEventIsNotNull) {
    cl_event eventReturned = NULL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskCount = 100;

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_FALSE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        &eventReturned,
        &retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, eventReturned);

    auto eventObject = castToObject<Event>(eventReturned);
    EXPECT_EQ(0u, eventObject->peekTaskCount());
    EXPECT_TRUE(eventObject->updateStatusAndCheckCompletion());

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        buffer,
        ptrResult,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(eventReturned);
    clReleaseMemObject(buffer);
}

TEST_F(EnqueueMapBufferTest, GivenZeroCopyBufferWhenMapBufferWithoutEventsThenCommandStreamReceiverUpdatesRequiredDCFlushCount) {
    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_FALSE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    clReleaseMemObject(buffer);
}

TEST_F(EnqueueMapBufferTest, givenBufferWithoutUseHostPtrFlagWhenMappedOnCpuThenSetAllMapParams) {
    std::unique_ptr<Buffer> buffer(Buffer::create(BufferDefaults::context, CL_MEM_READ_WRITE, 10, nullptr, retVal));
    EXPECT_NE(nullptr, buffer);
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    size_t mapSize = 3;
    size_t mapOffset = 2;

    auto mappedPtr = clEnqueueMapBuffer(pCmdQ, buffer.get(), CL_FALSE, CL_MAP_READ, mapOffset, mapSize, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);

    MapInfo mappedInfo;
    auto success = buffer->findMappedPtr(mappedPtr, mappedInfo);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, mappedInfo.ptr);

    EXPECT_EQ(mapOffset, mappedInfo.offset[0]);
    EXPECT_EQ(0u, mappedInfo.offset[1]);
    EXPECT_EQ(0u, mappedInfo.offset[2]);

    EXPECT_EQ(mapSize, mappedInfo.size[0]);
    EXPECT_EQ(0u, mappedInfo.size[1]);
    EXPECT_EQ(0u, mappedInfo.size[2]);

    auto expectedPtr = ptrOffset(buffer->getCpuAddressForMapping(), mapOffset);

    EXPECT_EQ(mappedPtr, expectedPtr);
}

TEST_F(EnqueueMapBufferTest, givenBufferWithUseHostPtrFlagWhenMappedOnCpuThenSetAllMapParams) {
    uint8_t hostPtr[10] = {};
    std::unique_ptr<Buffer> buffer(Buffer::create(BufferDefaults::context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, 10, hostPtr, retVal));
    EXPECT_NE(nullptr, buffer);
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());

    size_t mapSize = 3;
    size_t mapOffset = 2;

    auto mappedPtr = clEnqueueMapBuffer(pCmdQ, buffer.get(), CL_FALSE, CL_MAP_READ, mapOffset, mapSize, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, mappedPtr);

    MapInfo mappedInfo;
    auto success = buffer->findMappedPtr(mappedPtr, mappedInfo);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, mappedInfo.ptr);

    EXPECT_EQ(mapOffset, mappedInfo.offset[0]);
    EXPECT_EQ(0u, mappedInfo.offset[1]);
    EXPECT_EQ(0u, mappedInfo.offset[2]);

    EXPECT_EQ(mapSize, mappedInfo.size[0]);
    EXPECT_EQ(0u, mappedInfo.size[1]);
    EXPECT_EQ(0u, mappedInfo.size[2]);

    auto expectedPtr = ptrOffset(buffer->getCpuAddressForMapping(), mapOffset);

    EXPECT_EQ(mappedPtr, expectedPtr);
}
