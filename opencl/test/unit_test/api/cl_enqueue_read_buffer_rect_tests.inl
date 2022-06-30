/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueReadBufferRectTest;

namespace ULT {

TEST_F(clEnqueueReadBufferRectTest, GivenInvalidBufferWhenReadingRectangularRegionThenInvalidMemObjectErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;
    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
    char ptr[10];

    auto retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  //bufferRowPitch
        0,   //bufferSlicePitch
        10,  //hostRowPitch
        0,   //hostSlicePitch
        ptr, //hostPtr
        0,   //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueReadBufferRectTest, GivenNullCommandQueueWhenReadingRectangularRegionThenInvalidCommandQueueErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;
    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
    char ptr[10];

    auto retVal = clEnqueueReadBufferRect(
        nullptr,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  //bufferRowPitch
        0,   //bufferSlicePitch
        10,  //hostRowPitch
        0,   //hostSlicePitch
        ptr, //hostPtr
        0,   //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueReadBufferRectTest, GivenNullHostPtrWhenReadingRectangularRegionThenInvalidValueErrorIsReturned) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        100,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};

    auto retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,      //bufferRowPitch
        0,       //bufferSlicePitch
        10,      //hostRowPitch
        0,       //hostSlicePitch
        nullptr, //hostPtr
        0,       //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueReadBufferRectTest, GivenValidParametersWhenReadingRectangularRegionThenSuccessIsReturned) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        100,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    char ptr[10];

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    auto retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  //bufferRowPitch
        0,   //bufferSlicePitch
        10,  //hostRowPitch
        0,   //hostSlicePitch
        ptr, //hostPtr
        0,   //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseMemObject(buffer);
}

TEST_F(clEnqueueReadBufferRectTest, GivenQueueIncapableWhenReadingRectangularRegionThenInvalidOperationIsReturned) {
    MockBuffer buffer{};
    buffer.size = 100;
    char ptr[10];

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL);
    auto retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        &buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  //bufferRowPitch
        0,   //bufferSlicePitch
        10,  //hostRowPitch
        0,   //hostSlicePitch
        ptr, //hostPtr
        0,   //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clEnqueueReadBufferRectTest, GivenInvalidPitchWhenReadingRectangularRegionThenInvalidValueErrorIsReturned) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        100,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    char ptr[10];

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};
    size_t bufferRowPitch = 9;

    auto retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        bufferRowPitch, //bufferRowPitch
        0,              //bufferSlicePitch
        10,             //hostRowPitch
        0,              //hostSlicePitch
        ptr,            //hostPtr
        0,              //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    size_t hostRowPitch = 9;
    retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,           //bufferRowPitch
        0,            //bufferSlicePitch
        hostRowPitch, //hostRowPitch
        0,            //hostSlicePitch
        ptr,          //hostPtr
        0,            //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    size_t bufferSlicePitch = 9;
    retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,               //bufferRowPitch
        bufferSlicePitch, //bufferSlicePitch
        10,               //hostRowPitch
        0,                //hostSlicePitch
        ptr,              //hostPtr
        0,                //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    size_t hostSlicePitch = 9;
    retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,             //bufferRowPitch
        0,              //bufferSlicePitch
        10,             //hostRowPitch
        hostSlicePitch, //hostSlicePitch
        ptr,            //hostPtr
        0,              //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    clReleaseMemObject(buffer);
}

class EnqueueReadBufferRectFlagsTest : public ApiFixture<>,
                                       public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    EnqueueReadBufferRectFlagsTest() {
    }

  protected:
    void SetUp() override {
        ApiFixture::SetUp();
        buffer_flags = GetParam();

        unsigned int bufferSize = 16;
        pHostMem = new unsigned char[bufferSize];
        memset(pHostMem, 0xaa, bufferSize);

        buffer = clCreateBuffer(
            pContext,
            buffer_flags,
            bufferSize,
            pHostMem,
            &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, buffer);
    }

    void TearDown() override {
        retVal = clReleaseMemObject(buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);
        delete[] pHostMem;
        ApiFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    cl_mem_flags buffer_flags = 0;
    unsigned char *pHostMem;
    cl_mem buffer;
};

typedef EnqueueReadBufferRectFlagsTest EnqueueReadReadBufferRectTests;

TEST_P(EnqueueReadReadBufferRectTests, GivenNoReadFlagsWhenReadingRectangularRegionThenInvalidOperationErrorIsReturned) {
    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
    char ptr[10];
    cl_event eventReturned = nullptr;
    retVal = clEnqueueReadBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  //bufferRowPitch
        0,   //bufferSlicePitch
        10,  //hostRowPitch
        0,   //hostSlicePitch
        ptr, //hostPtr
        0,   //numEventsInWaitList
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    clReleaseEvent(eventReturned);
}

static cl_mem_flags read_buffer_rect_flags[] = {
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    EnqueueReadBufferRectFlagsTests_Create,
    EnqueueReadReadBufferRectTests,
    testing::ValuesIn(read_buffer_rect_flags));
} // namespace ULT
