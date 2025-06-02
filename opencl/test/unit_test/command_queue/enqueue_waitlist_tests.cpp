/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

class ClEventWrapper {
  public:
    ClEventWrapper() { mMem = NULL; }
    ClEventWrapper(cl_event mem) { mMem = mem; }
    ClEventWrapper(const ClEventWrapper &rhs) : mMem(rhs.mMem) {
        if (mMem != NULL)
            clRetainEvent(mMem);
    }
    ~ClEventWrapper() {
        if (mMem != NULL)
            clReleaseEvent(mMem);
    }
    ClEventWrapper &operator=(const cl_event &rhs) {
        mMem = rhs;
        return *this;
    }
    ClEventWrapper &operator=(ClEventWrapper rhs) {
        std::swap(mMem, rhs.mMem);
        return *this;
    }
    operator cl_event() const { return mMem; }
    cl_event *operator&() { return &mMem; }
    bool operator==(const cl_event &rhs) { return mMem == rhs; }

  protected:
    cl_event mMem;
};

using namespace NEO;

namespace ULT {

struct EnqueueWaitlistTest;

typedef HelloWorldTestWithParam<HelloWorldFixtureFactory> EnqueueWaitlistFixture;
typedef void (*ExecuteEnqueue)(EnqueueWaitlistTest *, uint32_t /*cl_uint*/, cl_event *, cl_event *, bool);

struct EnqueueWaitlistTest : public EnqueueWaitlistFixture,
                             public ::testing::TestWithParam<ExecuteEnqueue> {
  public:
    typedef CommandQueueHwFixture CommandQueueFixture;
    using CommandQueueHwFixture::pCmdQ;

    void SetUp() override {
        EnqueueWaitlistFixture::setUp();
        buffer = BufferHelper<>::create();
        bufferNonZeroCopy = new UnalignedBuffer(BufferDefaults::context, &bufferNonZeroCopyAlloc);
        image = Image1dHelperUlt<>::create(BufferDefaults::context);
        imageNonZeroCopy = ImageHelperUlt<ImageUseHostPtr<Image1dDefaults>>::create(BufferDefaults::context);
    }

    void TearDown() override {
        buffer->decRefInternal();
        bufferNonZeroCopy->decRefInternal();
        image->decRefInternal();
        imageNonZeroCopy->decRefInternal();
        EnqueueWaitlistFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    cl_int error = CL_SUCCESS;

    MockGraphicsAllocation bufferNonZeroCopyAlloc{nullptr, MemoryConstants::pageSize};
    Buffer *buffer = nullptr;
    Buffer *bufferNonZeroCopy = nullptr;
    Image *image = nullptr;
    Image *imageNonZeroCopy = nullptr;

    void testError(cl_int error, std::string str) {
        EXPECT_EQ(CL_SUCCESS, error) << str << std::endl;
    }

    static void enqueueNDRange(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        size_t threadNum = 10;
        size_t threads[1] = {threadNum};
        cl_int error = clEnqueueNDRangeKernel(test->pCmdQ, test->pMultiDeviceKernel, 1, NULL, threads, threads, numWaits, waits, outEvent);
        test->testError(error, "Unable to execute kernel");
        return;
    }

    static void enqueueMapBuffer(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        void *mappedPtr = clEnqueueMapBuffer(test->pCmdQ, test->buffer, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, 0, test->buffer->getSize(), numWaits, waits, outEvent, &error);
        EXPECT_NE(nullptr, mappedPtr);
        test->testError(error, "Unable to enqueue buffer map");
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->buffer, mappedPtr, 0, nullptr, nullptr);
        return;
    }

    static void twoEnqueueMapBuffer(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        void *mappedPtr = clEnqueueMapBuffer(test->pCmdQ, test->buffer, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, 0, test->buffer->getSize(), numWaits, waits, outEvent, &error);
        EXPECT_NE(nullptr, mappedPtr);
        test->testError(error, "Unable to enqueue buffer map");

        void *mappedPtr2 = clEnqueueMapBuffer(test->pCmdQ, test->bufferNonZeroCopy, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, 0, test->bufferNonZeroCopy->getSize(), 0, nullptr, nullptr, &error);
        EXPECT_NE(nullptr, mappedPtr2);
        test->testError(error, "Unable to enqueue buffer map");

        error = clEnqueueUnmapMemObject(test->pCmdQ, test->buffer, mappedPtr, 0, nullptr, nullptr);
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->bufferNonZeroCopy, mappedPtr2, 0, nullptr, nullptr);

        return;
    }
    static void enqueueUnMapBuffer(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        void *mappedPtr = clEnqueueMapBuffer(test->pCmdQ, test->buffer, CL_TRUE, CL_MAP_READ, 0, test->buffer->getSize(), 0, nullptr, nullptr, &error);
        EXPECT_NE(nullptr, mappedPtr);
        ASSERT_NE(test->buffer, nullptr);
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->buffer, mappedPtr, numWaits, waits, outEvent);
        test->testError(error, "Unable to unmap buffer");
        return;
    }
    static void enqueueMapImage(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        cl_image_desc desc = test->image->getImageDesc();
        size_t origin[3] = {0, 0, 0}, region[3] = {desc.image_width, desc.image_height, 1};
        size_t outPitch;

        void *mappedPtr = clEnqueueMapImage(test->pCmdQ, test->image, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, origin, region, &outPitch, NULL, numWaits, waits, outEvent, &error);
        test->testError(error, "Unable to enqueue image map");
        EXPECT_NE(nullptr, mappedPtr);
        test->testError(error, "Unable to enqueue buffer map");
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->image, mappedPtr, 0, nullptr, nullptr);
        return;
    }

    static void twoEnqueueMapImage(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        cl_image_desc desc = test->image->getImageDesc();
        size_t origin[3] = {0, 0, 0}, region[3] = {desc.image_width, desc.image_height, 1};
        size_t outPitch;

        size_t origin2[3] = {0, 0, 0}, region2[3] = {desc.image_width, desc.image_height, 1};
        size_t outPitch2;

        void *mappedPtr = clEnqueueMapImage(test->pCmdQ, test->image, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, origin, region, &outPitch, NULL, numWaits, waits, outEvent, &error);
        test->testError(error, "Unable to enqueue image map");
        EXPECT_NE(nullptr, mappedPtr);
        test->testError(error, "Unable to enqueue buffer map");

        void *mappedPtr2 = clEnqueueMapImage(test->pCmdQ, test->imageNonZeroCopy, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, origin2, region2, &outPitch2, NULL, 0, nullptr, nullptr, &error);
        test->testError(error, "Unable to enqueue image map");
        EXPECT_NE(nullptr, mappedPtr2);
        test->testError(error, "Unable to enqueue buffer map");

        error = clEnqueueUnmapMemObject(test->pCmdQ, test->image, mappedPtr, 0, nullptr, nullptr);
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->imageNonZeroCopy, mappedPtr2, 0, nullptr, nullptr);
        return;
    }
};

TEST_P(EnqueueWaitlistTest, GivenCompletedUserEventOnWaitlistWhenWaitingForOutputEventThenOutputEventIsCompleted) {

    // Set up a user event, which we use as a gate for the second event
    ClEventWrapper gateEvent = clCreateUserEvent(context, &error);
    testError(error, "Unable to set up user gate event");

    // Set up the execution of the action with its actual event
    ClEventWrapper actualEvent;

    // call the function to execute
    GetParam()(this, 1, &gateEvent, &actualEvent, false);

    // Now release the user event, which will allow our actual action to run
    error = clSetUserEventStatus(gateEvent, CL_COMPLETE);
    testError(error, "Unable to trigger gate event");

    // Now we wait for completion. Note that we can actually wait on the event itself, at least at first
    error = clWaitForEvents(1, &actualEvent);
    testError(error, "Unable to wait for actual test event");
}

typedef EnqueueWaitlistTest EnqueueWaitlistTestTwoMapEnqueues;
TEST_P(EnqueueWaitlistTestTwoMapEnqueues, GivenCompletedUserEventOnWaitlistWhenWaitingForOutputEventThenOutputEventIsCompleted) {

    // Set up a user event, which we use as a gate for the second event
    ClEventWrapper gateEvent = clCreateUserEvent(context, &error);
    testError(error, "Unable to set up user gate event");

    // Set up the execution of the action with its actual event
    ClEventWrapper actualEvent;

    // call the function to execute
    GetParam()(this, 1, &gateEvent, &actualEvent, false);

    // Now release the user event, which will allow our actual action to run
    error = clSetUserEventStatus(gateEvent, CL_COMPLETE);

    // Now we wait for completion. Note that we can actually wait on the event itself, at least at first
    error = clWaitForEvents(1, &actualEvent);
    testError(error, "Unable to wait for actual test event");
}

TEST_P(EnqueueWaitlistTest, GivenCompletedUserEventOnWaitlistWhenFinishingCommandQueueThenSuccessIsReturned) {

    // Set up a user event, which we use as a gate for the second event
    ClEventWrapper gateEvent = clCreateUserEvent(context, &error);
    testError(error, "Unable to set up user gate event");

    // call the function to execute
    GetParam()(this, 1, &gateEvent, nullptr, false);

    // Now release the user event, which will allow our actual action to run
    error = clSetUserEventStatus(gateEvent, CL_COMPLETE);
    testError(error, "Unable to trigger gate event");

    // Now we wait for completion. Note that we can actually wait on the event itself, at least at first
    error = clFinish(pCmdQ);
    testError(error, "Finish FAILED");
}

ExecuteEnqueue enqueues[] =
    {
        &EnqueueWaitlistTest::enqueueNDRange,
        &EnqueueWaitlistTest::enqueueMapBuffer,
        &EnqueueWaitlistTest::enqueueUnMapBuffer,
        &EnqueueWaitlistTest::enqueueMapImage};

ExecuteEnqueue twoEnqueueMap[] =
    {
        &EnqueueWaitlistTest::twoEnqueueMapBuffer,
        &EnqueueWaitlistTest::twoEnqueueMapImage};

INSTANTIATE_TEST_SUITE_P(
    UnblockedEvent,
    EnqueueWaitlistTest,
    ::testing::ValuesIn(enqueues));

INSTANTIATE_TEST_SUITE_P(
    TwoEnqueueMap,
    EnqueueWaitlistTestTwoMapEnqueues,
    ::testing::ValuesIn(twoEnqueueMap));
} // namespace ULT
