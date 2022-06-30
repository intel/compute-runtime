/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueMapBufferTests;

TEST_F(clEnqueueMapBufferTests, GivenNullCommandQueueWhenMappingBufferThenInvalidCommandQueueErrorIsReturned) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_event eventReturned = nullptr;
    auto ptrResult = clEnqueueMapBuffer(
        nullptr,
        buffer,
        CL_TRUE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        &eventReturned,
        &retVal);
    EXPECT_EQ(nullptr, ptrResult);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;

    clReleaseEvent(eventReturned);
}

TEST_F(clEnqueueMapBufferTests, GivenValidParametersWhenMappingBufferThenSuccessIsReturned) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_event eventReturned = nullptr;
    auto ptrResult = clEnqueueMapBuffer(
        pCommandQueue,
        buffer,
        CL_TRUE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        &eventReturned,
        &retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;

    clReleaseEvent(eventReturned);
}

TEST_F(clEnqueueMapBufferTests, GivenQueueIncapableWhenMappingBufferThenInvalidOperationIsReturned) {
    MockBuffer buffer{};

    disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL);
    auto ptrResult = clEnqueueMapBuffer(
        pCommandQueue,
        &buffer,
        CL_TRUE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        nullptr,
        &retVal);
    EXPECT_EQ(nullptr, ptrResult);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clEnqueueMapBufferTests, GivenMappedPointerWhenCreatingBufferFromThisPointerThenInvalidHostPtrErrorIsReturned) {
    unsigned int bufferSize = 16;

    cl_mem buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, bufferSize, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    void *hostPointer = clEnqueueMapBuffer(pCommandQueue, buffer, CL_TRUE, CL_MAP_READ, 0, bufferSize, 0, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, hostPointer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(pCommandQueue, buffer, hostPointer, 0, NULL, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferFromHostPtr = clCreateBuffer(pContext, CL_MEM_USE_HOST_PTR, bufferSize, hostPointer, &retVal);
    EXPECT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, bufferFromHostPtr);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

class EnqueueMapBufferFlagsTest : public ApiFixture<>,
                                  public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    EnqueueMapBufferFlagsTest() {
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

typedef EnqueueMapBufferFlagsTest EnqueueMapReadBufferTests;

TEST_P(EnqueueMapReadBufferTests, GivenInvalidFlagsWhenMappingBufferForReadingThenInvalidOperationErrorIsReturned) {
    cl_event eventReturned = nullptr;
    auto ptrResult = clEnqueueMapBuffer(
        pCommandQueue,
        buffer,
        CL_TRUE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        &eventReturned,
        &retVal);
    EXPECT_EQ(nullptr, ptrResult);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    clReleaseEvent(eventReturned);
}

static cl_mem_flags NoReadAccessFlags[] = {
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    EnqueueMapBufferFlagsTests_Create,
    EnqueueMapReadBufferTests,
    testing::ValuesIn(NoReadAccessFlags));

typedef EnqueueMapBufferFlagsTest EnqueueMapWriteBufferTests;

TEST_P(EnqueueMapWriteBufferTests, GivenInvalidFlagsWhenMappingBufferForWritingThenInvalidOperationErrorIsReturned) {
    cl_event eventReturned = nullptr;
    auto ptrResult = clEnqueueMapBuffer(
        pCommandQueue,
        buffer,
        CL_TRUE,
        CL_MAP_WRITE,
        0,
        8,
        0,
        nullptr,
        &eventReturned,
        &retVal);
    EXPECT_EQ(nullptr, ptrResult);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    clReleaseEvent(eventReturned);
}

static cl_mem_flags NoWriteAccessFlags[] = {
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_READ_ONLY,
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    EnqueueMapBufferFlagsTests_Create,
    EnqueueMapWriteBufferTests,
    testing::ValuesIn(NoWriteAccessFlags));
