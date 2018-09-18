/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"

using namespace OCLRT;

typedef api_tests clEnqueueMapBufferTests;

TEST_F(clEnqueueMapBufferTests, NullCommandQueue_returnsError) {
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

TEST_F(clEnqueueMapBufferTests, validInputs_returnsSuccess) {
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

TEST_F(clEnqueueMapBufferTests, GivenMappedPointerWhenCreateBufferFormThisPointerThenReturnInvalidHostPointer) {
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

class EnqueueMapBufferFlagsTest : public api_fixture,
                                  public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    EnqueueMapBufferFlagsTest() {
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

typedef EnqueueMapBufferFlagsTest EnqueueMapReadBufferTests;

TEST_P(EnqueueMapReadBufferTests, invalidFlags) {
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

static cl_mem_flags read_buffer_flags[] = {
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    EnqueueMapBufferFlagsTests_Create,
    EnqueueMapReadBufferTests,
    testing::ValuesIn(read_buffer_flags));

typedef EnqueueMapBufferFlagsTest EnqueueMapWriteBufferTests;

TEST_P(EnqueueMapWriteBufferTests, invalidFlags) {
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

static cl_mem_flags write_buffer_flags[] = {
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_READ_ONLY,
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    EnqueueMapBufferFlagsTests_Create,
    EnqueueMapWriteBufferTests,
    testing::ValuesIn(write_buffer_flags));
