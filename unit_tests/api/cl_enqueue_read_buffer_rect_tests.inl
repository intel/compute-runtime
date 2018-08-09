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

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"

using namespace OCLRT;

typedef api_tests clEnqueueReadBufferRectTest;

namespace ULT {

TEST_F(clEnqueueReadBufferRectTest, returnFailure) {
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

TEST_F(clEnqueueReadBufferRectTest, NullCommandQueue) {
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

TEST_F(clEnqueueReadBufferRectTest, NullHostPtr) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        20,
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

TEST_F(clEnqueueReadBufferRectTest, returnSuccess) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    char ptr[10];

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

TEST_F(clEnqueueReadBufferRectTest, pitchInvalid) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    char ptr[10];

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
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

class EnqueueReadBufferRectFlagsTest : public api_fixture,
                                       public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    EnqueueReadBufferRectFlagsTest() {
    }

  protected:
    void SetUp() override {
        api_fixture::SetUp();
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
        api_fixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    cl_mem_flags buffer_flags = 0;
    unsigned char *pHostMem;
    cl_mem buffer;
};

typedef EnqueueReadBufferRectFlagsTest EnqueueReadReadBufferRectTests;

TEST_P(EnqueueReadReadBufferRectTests, invalidFlags) {
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
