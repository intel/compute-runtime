/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "cl_api_tests.h"

using namespace NEO;

struct clEnqueueCopyBufferRectTests : public ApiFixture<0>, ::testing::Test {
    void SetUp() override {
        ApiFixture::setUp();
    }
    void TearDown() override {
        ApiFixture::tearDown();
    }
};

namespace ULT {

TEST_F(clEnqueueCopyBufferRectTests, GivenCorrectParametersWhenEnqueingCopyBufferRectThenSuccessIsReturned) {
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    auto retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, GivenNullCommandQueueWhenEnqueingCopyBufferRectThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueCopyBufferRect(
        nullptr, //command_queue
        nullptr, //srcBuffer
        nullptr, //dstBuffer
        nullptr, //srcOrigin
        nullptr, //dstOrigin
        nullptr, //retion
        0,       //srcRowPitch
        0,       //srcSlicePitch
        0,       //dstRowPitch
        0,       //dstSlicePitch
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, GivenQueueIncapableWhenEnqueingCopyBufferRectThenInvalidOperationIsReturned) {
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL);
    auto retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, givenPitchesEqualZeroAndZerosInRegionWhenCallClEnqueueCopyBufferRectThenClInvalidValueIsReturned) {
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {0, 0, 0};

    auto retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        0, //srcRowPitch
        0, //srcSlicePitch
        0, //dstRowPitch
        0, //dstSlicePitch
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, givenZeroInRegionWhenCallClEnqueueCopyBufferRectThenClInvalidValueIsReturned) {
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {0, 0, 0};

    auto retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    size_t region1[] = {10, 10, 0};

    retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region1,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    size_t region2[] = {10, 0, 1};

    retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region2,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    size_t region3[] = {10, 10, 0};

    retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region3,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, givenNonProperSrcBufferSizeWhenCallClEnqueueCopyBufferRectThenClInvalidValueIsReturned) {
    MockBuffer srcBuffer;
    srcBuffer.size = 10;
    MockBuffer dstBuffer;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    auto retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, givenNonProperDstBufferSizeWhenCallClEnqueueCopyBufferRectThenClInvalidValueIsReturned) {
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    dstBuffer.size = 10;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    auto retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, givenPitchesEqualZeroAndNotZeroRegionWhenCallClEnqueueCopyBufferRectThenPitchIsSetBasedOnRegionAndClSuccessIsReturned) {
    class CommandQueueMock : public MockCommandQueue {
      public:
        CommandQueueMock(Context *context, ClDevice *device, const cl_queue_properties *props) : MockCommandQueue(context, device, props, false) {}
        cl_int enqueueCopyBufferRect(Buffer *srcBuffer, Buffer *dstBuffer, const size_t *srcOrigin, const size_t *dstOrigin,
                                     const size_t *region, size_t argSrcRowPitch, size_t argSrcSlicePitch, size_t argDstRowPitch,
                                     size_t argDstSlicePitch, cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList, cl_event *event) override {
            srcRowPitch = argSrcRowPitch;
            srcSlicePitch = argSrcSlicePitch;
            dstRowPitch = argDstRowPitch;
            dstSlicePitch = argDstSlicePitch;
            return CL_SUCCESS;
        }

        size_t srcRowPitch;
        size_t srcSlicePitch;
        size_t dstRowPitch;
        size_t dstSlicePitch;
    };

    auto commandQueue = std::make_unique<CommandQueueMock>(pContext, pDevice, nullptr);
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    dstBuffer.size = 200;
    srcBuffer.size = 200;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 20, 1};

    auto retVal = clEnqueueCopyBufferRect(
        commandQueue.get(),
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        0, //srcRowPitch
        0, //srcSlicePitch
        0, //dstRowPitch
        0, //dstSlicePitch
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(region[0], commandQueue->srcRowPitch);
    EXPECT_EQ(region[0], commandQueue->dstRowPitch);

    EXPECT_EQ(region[1], commandQueue->srcSlicePitch / commandQueue->srcRowPitch);
    EXPECT_EQ(region[1], commandQueue->dstSlicePitch / commandQueue->dstRowPitch);
}
} // namespace ULT
