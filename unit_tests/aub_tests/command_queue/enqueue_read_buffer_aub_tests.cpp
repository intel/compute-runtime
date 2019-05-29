/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/aub_tests/aub_tests_configuration.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include <memory>

using namespace NEO;

struct ReadBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<size_t>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

typedef ReadBufferHw AUBReadBuffer;

HWTEST_P(AUBReadBuffer, simple) {
    MockContext context(this->pDevice);

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

    allocation = pCommandStreamReceiver->getTemporaryAllocations().peekHead();
    while (allocation && allocation->getUnderlyingBuffer() != pDestMemory) {
        allocation = allocation->next;
    }
    retVal = pCmdQ->flush();
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSrcMemory = ptrOffset(pSrcMemory, offset);

    cl_float *destGpuaddress = reinterpret_cast<cl_float *>(allocation->getGpuAddress());
    // Compute our memory expecations based on kernel execution
    size_t sizeUserMemory = sizeof(destMemory);
    AUBCommandStreamFixture::expectMemory<FamilyType>(destGpuaddress, pSrcMemory, sizeWritten);

    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    if (offset + sizeWritten < sizeUserMemory) {
        pDestMemory = ptrOffset(pDestMemory, sizeWritten);
        destGpuaddress = ptrOffset(destGpuaddress, sizeWritten);

        size_t sizeRemaining = sizeUserMemory - sizeWritten - offset;
        AUBCommandStreamFixture::expectMemory<FamilyType>(destGpuaddress, pDestMemory, sizeRemaining);
    }
}

INSTANTIATE_TEST_CASE_P(AUBReadBuffer_simple,
                        AUBReadBuffer,
                        ::testing::Values(
                            0 * sizeof(cl_float),
                            1 * sizeof(cl_float),
                            2 * sizeof(cl_float),
                            3 * sizeof(cl_float)));

HWTEST_F(AUBReadBuffer, reserveCanonicalGpuAddress) {
    if (!GetAubTestsConfig<FamilyType>().testCanonicalAddress) {
        return;
    }

    MockContext context(this->pDevice);

    cl_float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    cl_float dstMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    GraphicsAllocation *srcAlocation = new GraphicsAllocation(GraphicsAllocation::AllocationType::UNKNOWN,
                                                              srcMemory,
                                                              0xFFFF800400001000,
                                                              0xFFFF800400001000,
                                                              sizeof(srcMemory),
                                                              MemoryPool::MemoryNull,
                                                              false);

    std::unique_ptr<Buffer> srcBuffer(Buffer::createBufferHw(&context,
                                                             CL_MEM_USE_HOST_PTR,
                                                             sizeof(srcMemory),
                                                             srcAlocation->getUnderlyingBuffer(),
                                                             srcMemory,
                                                             srcAlocation,
                                                             false,
                                                             false,
                                                             false));
    ASSERT_NE(nullptr, srcBuffer);

    srcBuffer->forceDisallowCPUCopy = true;
    auto retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                           CL_TRUE,
                                           0,
                                           sizeof(dstMemory),
                                           dstMemory,
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    GraphicsAllocation *dstAllocation = createResidentAllocationAndStoreItInCsr(dstMemory, sizeof(dstMemory));
    cl_float *dstGpuAddress = reinterpret_cast<cl_float *>(dstAllocation->getGpuAddress());

    AUBCommandStreamFixture::expectMemory<FamilyType>(dstGpuAddress, srcMemory, sizeof(dstMemory));
}

struct AUBReadBufferUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }

    template <typename FamilyType>
    void testReadBufferUnaligned(size_t offset, size_t size) {
        MockContext context(&pCmdQ->getDevice());

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
            CL_TRUE,
            offset,
            size,
            ptrOffset(dstMemory, offset),
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        // Check the memory
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset), ptrOffset(srcMemory, offset), size);
    }
};

HWTEST_F(AUBReadBufferUnaligned, all) {
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {4, 3, 2, 1};
    for (auto offset : offsets) {
        for (auto size : sizes) {
            testReadBufferUnaligned<FamilyType>(offset, size);
        }
    }
}
