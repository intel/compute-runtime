/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <memory>

using namespace NEO;

struct ReadBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<size_t>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::setUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::tearDown();
    }
};

typedef ReadBufferHw AUBReadBuffer;

HWTEST_P(AUBReadBuffer, WhenReadingBufferThenExpectationsAreMet) {
    MockContext context(this->pClDevice);

    cl_float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    cl_float destMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(srcMemory),
        srcMemory,
        retVal));
    ASSERT_NE(nullptr, srcBuffer.get());

    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = &destMemory[0];

    cl_bool blockingRead = CL_FALSE;
    size_t offset = GetParam();
    size_t sizeWritten = sizeof(cl_float);
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    GraphicsAllocation *allocation = createResidentAllocationAndStoreItInCsr(pDestMemory, sizeof(destMemory));

    srcBuffer->forceDisallowCPUCopy = true;
    retVal = pCmdQ->enqueueReadBuffer(
        srcBuffer.get(),
        blockingRead,
        offset,
        sizeWritten,
        pDestMemory,
        nullptr,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    allocation = csr->getTemporaryAllocations().peekHead();
    while (allocation && allocation->getUnderlyingBuffer() != pDestMemory) {
        allocation = allocation->next;
    }
    retVal = pCmdQ->flush();
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSrcMemory = ptrOffset(pSrcMemory, offset);

    cl_float *destGpuaddress = reinterpret_cast<cl_float *>(allocation->getGpuAddress());
    // Compute our memory expectations based on kernel execution
    size_t sizeUserMemory = sizeof(destMemory);
    expectMemory<FamilyType>(destGpuaddress, pSrcMemory, sizeWritten);

    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    if (offset + sizeWritten < sizeUserMemory) {
        pDestMemory = ptrOffset(pDestMemory, sizeWritten);
        destGpuaddress = ptrOffset(destGpuaddress, sizeWritten);

        size_t sizeRemaining = sizeUserMemory - sizeWritten - offset;
        expectMemory<FamilyType>(destGpuaddress, pDestMemory, sizeRemaining);
    }
}

INSTANTIATE_TEST_SUITE_P(AUBReadBuffer_simple,
                         AUBReadBuffer,
                         ::testing::Values(
                             0 * sizeof(cl_float),
                             1 * sizeof(cl_float),
                             2 * sizeof(cl_float),
                             3 * sizeof(cl_float)));

HWTEST_F(AUBReadBuffer, GivenReserveCanonicalGpuAddressWhenReadingBufferThenExpectationsAreMet) {
    if (!getAubTestsConfig<FamilyType>().testCanonicalAddress) {
        return;
    }

    MockContext context(this->pClDevice);

    cl_float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    cl_float dstMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    GraphicsAllocation *srcAllocation = new MockGraphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown,
                                                                   srcMemory,
                                                                   0xFFFF800400001000,
                                                                   0xFFFF800400001000,
                                                                   sizeof(srcMemory),
                                                                   MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    std::unique_ptr<Buffer> srcBuffer(Buffer::createBufferHw(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_USE_HOST_PTR,
        0,
        sizeof(srcMemory),
        srcAllocation->getUnderlyingBuffer(),
        srcMemory,
        GraphicsAllocationHelper::toMultiGraphicsAllocation(srcAllocation),
        false,
        false,
        false));
    ASSERT_NE(nullptr, srcBuffer);

    srcBuffer->forceDisallowCPUCopy = true;
    auto retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                           CL_FALSE,
                                           0,
                                           sizeof(dstMemory),
                                           dstMemory,
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->flush();
    EXPECT_EQ(CL_SUCCESS, retVal);

    GraphicsAllocation *dstAllocation = createResidentAllocationAndStoreItInCsr(dstMemory, sizeof(dstMemory));
    cl_float *dstGpuAddress = reinterpret_cast<cl_float *>(dstAllocation->getGpuAddress());

    expectMemory<FamilyType>(dstGpuAddress, srcMemory, sizeof(dstMemory));
}

struct AUBReadBufferUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::setUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::tearDown();
    }

    template <typename FamilyType>
    void testReadBufferUnaligned(size_t offset, size_t size) {
        MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

        char srcMemory[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const auto bufferSize = sizeof(srcMemory);
        char dstMemory[bufferSize] = {0};

        auto retVal = CL_INVALID_VALUE;

        auto buffer = std::unique_ptr<Buffer>(Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            bufferSize,
            srcMemory,
            retVal));
        ASSERT_NE(nullptr, buffer);

        buffer->forceDisallowCPUCopy = true;

        // Map destination memory to GPU
        GraphicsAllocation *allocation = createResidentAllocationAndStoreItInCsr(dstMemory, bufferSize);
        auto dstMemoryGPUPtr = reinterpret_cast<char *>(allocation->getGpuAddress());

        // Do unaligned read
        retVal = pCmdQ->enqueueReadBuffer(
            buffer.get(),
            CL_FALSE,
            offset,
            size,
            ptrOffset(dstMemory, offset),
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pCmdQ->flush();
        EXPECT_EQ(CL_SUCCESS, retVal);

        // Check the memory
        expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset), ptrOffset(srcMemory, offset), size);
    }
};

HWTEST_F(AUBReadBufferUnaligned, GivenOffestAndSizeWhenReadingBufferThenExpectationsAreMet) {
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {4, 3, 2, 1};
    for (auto offset : offsets) {
        for (auto size : sizes) {
            testReadBufferUnaligned<FamilyType>(offset, size);
        }
    }
}
