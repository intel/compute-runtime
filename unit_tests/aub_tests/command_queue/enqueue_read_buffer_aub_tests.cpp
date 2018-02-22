/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/aub_tests/aub_tests_configuration.h"
#include "test.h"

using namespace OCLRT;

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
    MockContext context;

    cl_float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    cl_float destMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(srcMemory),
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, srcBuffer);

    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = &destMemory[0];

    cl_bool blockingRead = CL_TRUE;
    size_t offset = GetParam();
    size_t sizeWritten = sizeof(cl_float);
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    GraphicsAllocation *allocation = pCommandStreamReceiver->createAllocationAndHandleResidency(pDestMemory, sizeof(destMemory));

    srcBuffer->forceDisallowCPUCopy = true;
    retVal = pCmdQ->enqueueReadBuffer(
        srcBuffer,
        blockingRead,
        offset,
        sizeWritten,
        pDestMemory,
        numEventsInWaitList,
        eventWaitList,
        event);

    delete srcBuffer;

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

    MockContext context;

    cl_float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    cl_float dstMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    GraphicsAllocation *srcAlocation = new GraphicsAllocation(srcMemory, 0xFFFF800400001000, 0xFFFF800400001000, sizeof(srcMemory));

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
                                           0,
                                           nullptr,
                                           nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    GraphicsAllocation *dstAllocation = pCommandStreamReceiver->createAllocationAndHandleResidency(dstMemory, sizeof(dstMemory));
    cl_float *dstGpuAddress = reinterpret_cast<cl_float *>(dstAllocation->getGpuAddress());

    AUBCommandStreamFixture::expectMemory<FamilyType>(dstGpuAddress, srcMemory, sizeof(dstMemory));
}
