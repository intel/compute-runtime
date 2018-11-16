/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/allocations_list.h"
#include "unit_tests/aub_tests/command_queue/enqueue_write_copy_read_buffer_aub_tests.h"

#include "test.h"

using namespace OCLRT;

template <typename FamilyType>
void AubWriteCopyReadBuffer::runTest() {
    auto aubCsr = AUBFixture::getAubCsr<FamilyType>();

    char srcMemoryInitial[] = {1, 2, 3, 4, 5, 6, 7, 8};
    char dstMemoryInitial[] = {11, 12, 13, 14, 15, 16, 17, 18};

    char srcMemoryToWrite[] = {1, 2, 3, 4, 5, 6, 7, 8};
    char dstMemoryToWrite[] = {11, 12, 13, 14, 15, 16, 17, 18};

    const size_t bufferSize = sizeof(srcMemoryInitial);

    static_assert(bufferSize == sizeof(dstMemoryInitial), "");
    static_assert(bufferSize == sizeof(srcMemoryToWrite), "");
    static_assert(bufferSize == sizeof(dstMemoryToWrite), "");

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(
        context,
        CL_MEM_COPY_HOST_PTR,
        bufferSize,
        srcMemoryInitial,
        retVal));
    ASSERT_NE(nullptr, srcBuffer);

    auto dstBuffer = std::unique_ptr<Buffer>(Buffer::create(
        context,
        CL_MEM_COPY_HOST_PTR,
        bufferSize,
        dstMemoryInitial,
        retVal));
    ASSERT_NE(nullptr, dstBuffer);

    aubCsr->writeMemory(*srcBuffer->getGraphicsAllocation());
    aubCsr->writeMemory(*dstBuffer->getGraphicsAllocation());

    getAubCsr<FamilyType>()->expectMemoryEqual(AUBFixture::getGpuPointer(srcBuffer->getGraphicsAllocation()), srcMemoryInitial, bufferSize);
    getAubCsr<FamilyType>()->expectMemoryEqual(AUBFixture::getGpuPointer(dstBuffer->getGraphicsAllocation()), dstMemoryInitial, bufferSize);

    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    retVal = pCmdQ->enqueueWriteBuffer(
        srcBuffer.get(),
        true,
        0,
        bufferSize,
        srcMemoryToWrite,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->enqueueWriteBuffer(
        dstBuffer.get(),
        true,
        0,
        bufferSize,
        dstMemoryToWrite,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    getAubCsr<FamilyType>()->expectMemoryEqual(AUBFixture::getGpuPointer(srcBuffer->getGraphicsAllocation()), srcMemoryToWrite, bufferSize);
    getAubCsr<FamilyType>()->expectMemoryEqual(AUBFixture::getGpuPointer(dstBuffer->getGraphicsAllocation()), dstMemoryToWrite, bufferSize);

    retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        bufferSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    // Destination buffer should have src buffer content
    getAubCsr<FamilyType>()->expectMemoryEqual(AUBFixture::getGpuPointer(dstBuffer->getGraphicsAllocation()), srcMemoryToWrite, bufferSize);

    char hostPtrMemory[] = {0, 0, 0, 0, 0, 0, 0, 0};
    ASSERT_EQ(bufferSize, sizeof(hostPtrMemory));

    retVal = pCmdQ->enqueueReadBuffer(
        dstBuffer.get(),
        false,
        0,
        bufferSize,
        hostPtrMemory,
        numEventsInWaitList,
        eventWaitList,
        event);

    pCmdQ->flush();

    GraphicsAllocation *allocation = csr->getTemporaryAllocations().peekHead();
    while (allocation && allocation->getUnderlyingBuffer() != hostPtrMemory) {
        allocation = allocation->next;
    }

    getAubCsr<FamilyType>()->expectMemoryEqual(AUBFixture::getGpuPointer(allocation), srcMemoryToWrite, bufferSize);
}

HWTEST_F(AubWriteCopyReadBuffer, givenTwoBuffersFilledWithPatternWhenSourceIsCopiedToDestinationThenDestinationDataValidates) {
    runTest<FamilyType>();
}
