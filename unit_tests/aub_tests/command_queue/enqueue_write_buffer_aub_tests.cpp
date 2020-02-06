/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device/device.h"
#include "core/helpers/ptr_math.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

struct WriteBufferHw
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

typedef WriteBufferHw AUBWriteBuffer;

HWTEST_P(AUBWriteBuffer, simple) {
    MockContext context(this->pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

    cl_float *srcMemory = new float[1024];
    cl_float *destMemory = new float[1024];
    cl_float *zeroMemory = new float[1024];

    for (int i = 0; i < 1024; i++) {
        srcMemory[i] = (float)i + 1.0f;
        destMemory[i] = 0;
        zeroMemory[i] = 0;
    }

    auto retVal = CL_INVALID_VALUE;
    auto dstBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        1024 * sizeof(float),
        destMemory,
        retVal);
    ASSERT_NE(nullptr, dstBuffer);

    auto pSrcMemory = &srcMemory[0];

    cl_bool blockingWrite = CL_TRUE;
    size_t offset = GetParam();
    size_t sizeWritten = sizeof(cl_float);
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    dstBuffer->forceDisallowCPUCopy = true;
    retVal = pCmdQ->enqueueWriteBuffer(
        dstBuffer,
        blockingWrite,
        offset,
        sizeWritten,
        pSrcMemory,
        nullptr,
        numEventsInWaitList,
        eventWaitList,
        event);

    auto pDestMemory = reinterpret_cast<decltype(destMemory)>((dstBuffer->getGraphicsAllocation()->getGpuAddress()));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);

    // Compute our memory expecations based on kernel execution
    size_t sizeUserMemory = 1024 * sizeof(float);
    auto pVal = ptrOffset(pDestMemory, offset);
    AUBCommandStreamFixture::expectMemory<FamilyType>(pVal, pSrcMemory, sizeWritten);

    // if offset provided, check the beginning
    if (offset > 0) {
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, zeroMemory, offset);
    }
    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    if (offset + sizeWritten < sizeUserMemory) {
        pDestMemory = ptrOffset(pVal, sizeWritten);

        size_t sizeRemaining = sizeUserMemory - sizeWritten - offset;
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, zeroMemory, sizeRemaining);
    }
    delete dstBuffer;
    delete[] srcMemory;
    delete[] destMemory;
    delete[] zeroMemory;
}

INSTANTIATE_TEST_CASE_P(AUBWriteBuffer_simple,
                        AUBWriteBuffer,
                        ::testing::Values(
                            0 * sizeof(cl_float),
                            1 * sizeof(cl_float),
                            2 * sizeof(cl_float),
                            3 * sizeof(cl_float)));

struct AUBWriteBufferUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }

    template <typename FamilyType>
    void testWriteBufferUnaligned(size_t offset, size_t size) {
        MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

        char srcMemory[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const auto bufferSize = sizeof(srcMemory);
        char dstMemory[bufferSize] = {0};

        auto retVal = CL_INVALID_VALUE;

        auto buffer = std::unique_ptr<Buffer>(Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            bufferSize,
            dstMemory,
            retVal));
        ASSERT_NE(nullptr, buffer);

        buffer->forceDisallowCPUCopy = true;

        // Do unaligned write
        retVal = pCmdQ->enqueueWriteBuffer(
            buffer.get(),
            CL_TRUE,
            offset,
            size,
            ptrOffset(srcMemory, offset),
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        // Check the memory
        auto bufferGPUPtr = reinterpret_cast<char *>((buffer->getGraphicsAllocation()->getGpuAddress()));
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(bufferGPUPtr, offset), ptrOffset(srcMemory, offset), size);
    }
};

HWTEST_F(AUBWriteBufferUnaligned, all) {
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {4, 3, 2, 1};
    for (auto offset : offsets) {
        for (auto size : sizes) {
            testWriteBufferUnaligned<FamilyType>(offset, size);
        }
    }
}
