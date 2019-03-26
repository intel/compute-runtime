/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"

class clEventWrapper {
  public:
    clEventWrapper() { mMem = NULL; }
    clEventWrapper(cl_event mem) { mMem = mem; }
    clEventWrapper(const clEventWrapper &rhs) : mMem(rhs.mMem) {
        if (mMem != NULL)
            clRetainEvent(mMem);
    }
    ~clEventWrapper() {
        if (mMem != NULL)
            clReleaseEvent(mMem);
    }
    clEventWrapper &operator=(const cl_event &rhs) {
        mMem = rhs;
        return *this;
    }
    clEventWrapper &operator=(clEventWrapper rhs) {
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

    EnqueueWaitlistTest(void) {
        buffer = nullptr;
    }

    void SetUp() override {
        EnqueueWaitlistFixture::SetUp();
        buffer = BufferHelper<>::create();
        bufferNonZeroCopy = new UnalignedBuffer;
        image = Image1dHelper<>::create(BufferDefaults::context);
        imageNonZeroCopy = ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(BufferDefaults::context);
    }

    void TearDown() override {
        buffer->decRefInternal();
        bufferNonZeroCopy->decRefInternal();
        image->decRefInternal();
        imageNonZeroCopy->decRefInternal();
        EnqueueWaitlistFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    cl_int error = CL_SUCCESS;

    Buffer *buffer;
    Buffer *bufferNonZeroCopy;
    Image *image;
    Image *imageNonZeroCopy;

    void test_error(cl_int error, std::string str) {
        EXPECT_EQ(CL_SUCCESS, error) << str << std::endl;
    }

    static void EnqueueNDRange(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        size_t threadNum = 10;
        size_t threads[1] = {threadNum};
        cl_int error = clEnqueueNDRangeKernel(test->pCmdQ, test->pKernel, 1, NULL, threads, threads, numWaits, waits, outEvent);
        test->test_error(error, "Unable to execute kernel");
        return;
    }

    static void EnqueueMapBuffer(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        void *mappedPtr = clEnqueueMapBuffer(test->pCmdQ, test->buffer, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, 0, test->buffer->getSize(), numWaits, waits, outEvent, &error);
        EXPECT_NE(nullptr, mappedPtr);
        test->test_error(error, "Unable to enqueue buffer map");
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->buffer, mappedPtr, 0, nullptr, nullptr);
        return;
    }

    static void TwoEnqueueMapBuffer(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        void *mappedPtr = clEnqueueMapBuffer(test->pCmdQ, test->buffer, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, 0, test->buffer->getSize(), numWaits, waits, outEvent, &error);
        EXPECT_NE(nullptr, mappedPtr);
        test->test_error(error, "Unable to enqueue buffer map");

        void *mappedPtr2 = clEnqueueMapBuffer(test->pCmdQ, test->bufferNonZeroCopy, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, 0, test->bufferNonZeroCopy->getSize(), 0, nullptr, nullptr, &error);
        EXPECT_NE(nullptr, mappedPtr2);
        test->test_error(error, "Unable to enqueue buffer map");

        error = clEnqueueUnmapMemObject(test->pCmdQ, test->buffer, mappedPtr, 0, nullptr, nullptr);
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->bufferNonZeroCopy, mappedPtr2, 0, nullptr, nullptr);

        return;
    }
    static void EnqueueUnMapBuffer(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        void *mappedPtr = clEnqueueMapBuffer(test->pCmdQ, test->buffer, CL_TRUE, CL_MAP_READ, 0, test->buffer->getSize(), 0, nullptr, nullptr, &error);
        EXPECT_NE(nullptr, mappedPtr);
        ASSERT_NE(test->buffer, nullptr);
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->buffer, mappedPtr, numWaits, waits, outEvent);
        test->test_error(error, "Unable to unmap buffer");
        return;
    }
    static void EnqueueMapImage(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        cl_image_desc desc = test->image->getImageDesc();
        size_t origin[3] = {0, 0, 0}, region[3] = {desc.image_width, desc.image_height, 1};
        size_t outPitch;

        void *mappedPtr = clEnqueueMapImage(test->pCmdQ, test->image, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, origin, region, &outPitch, NULL, numWaits, waits, outEvent, &error);
        test->test_error(error, "Unable to enqueue image map");
        EXPECT_NE(nullptr, mappedPtr);
        test->test_error(error, "Unable to enqueue buffer map");
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->image, mappedPtr, 0, nullptr, nullptr);
        return;
    }

    static void TwoEnqueueMapImage(EnqueueWaitlistTest *test, cl_uint numWaits, cl_event *waits, cl_event *outEvent, bool blocking = false) {
        cl_int error;
        cl_image_desc desc = test->image->getImageDesc();
        size_t origin[3] = {0, 0, 0}, region[3] = {desc.image_width, desc.image_height, 1};
        size_t outPitch;

        size_t origin2[3] = {0, 0, 0}, region2[3] = {desc.image_width, desc.image_height, 1};
        size_t outPitch2;

        void *mappedPtr = clEnqueueMapImage(test->pCmdQ, test->image, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, origin, region, &outPitch, NULL, numWaits, waits, outEvent, &error);
        test->test_error(error, "Unable to enqueue image map");
        EXPECT_NE(nullptr, mappedPtr);
        test->test_error(error, "Unable to enqueue buffer map");

        void *mappedPtr2 = clEnqueueMapImage(test->pCmdQ, test->imageNonZeroCopy, blocking ? CL_TRUE : CL_FALSE, CL_MAP_READ, origin2, region2, &outPitch2, NULL, 0, nullptr, nullptr, &error);
        test->test_error(error, "Unable to enqueue image map");
        EXPECT_NE(nullptr, mappedPtr2);
        test->test_error(error, "Unable to enqueue buffer map");

        error = clEnqueueUnmapMemObject(test->pCmdQ, test->image, mappedPtr, 0, nullptr, nullptr);
        error = clEnqueueUnmapMemObject(test->pCmdQ, test->imageNonZeroCopy, mappedPtr2, 0, nullptr, nullptr);
        return;
    }
};

TEST_P(EnqueueWaitlistTest, BlockingWaitlist) {

    // Set up a user event, which we use as a gate for the second event
    clEventWrapper gateEvent = clCreateUserEvent(context, &error);
    test_error(error, "Unable to set up user gate event");

    // Set up the execution of the action with its actual event
    clEventWrapper actualEvent;

    // call the function to execute
    GetParam()(this, 1, &gateEvent, &actualEvent, false);

    // Now release the user event, which will allow our actual action to run
    error = clSetUserEventStatus(gateEvent, CL_COMPLETE);
    test_error(error, "Unable to trigger gate event");

    // Now we wait for completion. Note that we can actually wait on the event itself, at least at first
    error = clWaitForEvents(1, &actualEvent);
    test_error(error, "Unable to wait for actual test event");
}

typedef EnqueueWaitlistTest EnqueueWaitlistTestTwoMapEnqueues;
TEST_P(EnqueueWaitlistTestTwoMapEnqueues, TestPreviousVirtualEvent) {

    // Set up a user event, which we use as a gate for the second event
    clEventWrapper gateEvent = clCreateUserEvent(context, &error);
    test_error(error, "Unable to set up user gate event");

    // Set up the execution of the action with its actual event
    clEventWrapper actualEvent;

    // call the function to execute
    GetParam()(this, 1, &gateEvent, &actualEvent, false);

    // Now release the user event, which will allow our actual action to run
    error = clSetUserEventStatus(gateEvent, CL_COMPLETE);

    // Now we wait for completion. Note that we can actually wait on the event itself, at least at first
    error = clWaitForEvents(1, &actualEvent);
    test_error(error, "Unable to wait for actual test event");
}

TEST_P(EnqueueWaitlistTest, BlockingWaitlistNoOutEvent) {

    // Set up a user event, which we use as a gate for the second event
    clEventWrapper gateEvent = clCreateUserEvent(context, &error);
    test_error(error, "Unable to set up user gate event");

    // call the function to execute
    GetParam()(this, 1, &gateEvent, nullptr, false);

    // Now release the user event, which will allow our actual action to run
    error = clSetUserEventStatus(gateEvent, CL_COMPLETE);
    test_error(error, "Unable to trigger gate event");

    // Now we wait for completion. Note that we can actually wait on the event itself, at least at first
    error = clFinish(pCmdQ);
    test_error(error, "Finish FAILED");
}

ExecuteEnqueue Enqueues[] =
    {
        &EnqueueWaitlistTest::EnqueueNDRange,
        &EnqueueWaitlistTest::EnqueueMapBuffer,
        &EnqueueWaitlistTest::EnqueueUnMapBuffer,
        &EnqueueWaitlistTest::EnqueueMapImage};

ExecuteEnqueue TwoEnqueueMap[] =
    {
        &EnqueueWaitlistTest::TwoEnqueueMapBuffer,
        &EnqueueWaitlistTest::TwoEnqueueMapImage};

INSTANTIATE_TEST_CASE_P(
    UnblockedEvent,
    EnqueueWaitlistTest,
    ::testing::ValuesIn(Enqueues));

INSTANTIATE_TEST_CASE_P(
    TwoEnqueueMap,
    EnqueueWaitlistTestTwoMapEnqueues,
    ::testing::ValuesIn(TwoEnqueueMap));
} // namespace ULT
