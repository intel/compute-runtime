/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "opencl/source/cl_device/cl_device_info.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueReadBufferTests = ApiTests;

namespace ULT {

TEST_F(ClEnqueueReadBufferTests, GivenCorrectArgumentsWhenReadingBufferThenSuccessIsReturned) {
    MockContext context{};
    MockGraphicsAllocation allocation{};
    MockBuffer buffer{&context, allocation};
    MockCommandQueue commandQueue{context};
    auto data = 1;
    auto retVal = clEnqueueReadBuffer(
        &commandQueue,
        &buffer,
        false,
        0,
        sizeof(data),
        &data,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueReadBufferTests, GivenNonUsmMemoryWhenReadingBufferThenNonUsmPointerIsNotImported) {
    DebugManagerStateRestore restorer{};
    // 1KB staging buffer size
    debugManager.flags.StagingBufferSize.set(1);
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);

    MockContext context{};
    MockGraphicsAllocation allocation{};
    MockBuffer buffer{&context, allocation};
    MockCommandQueue commandQueue{context};
    char data[2048];
    auto retVal = clEnqueueReadBuffer(
        &commandQueue,
        &buffer,
        false,
        0,
        sizeof(data),
        data,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    // 2 * 1KB copies
    EXPECT_EQ(2u, commandQueue.enqueueReadBufferImplCalledCount);
}

TEST_F(ClEnqueueReadBufferTests, GivenQueueIncapableArgumentsWhenReadingBufferThenInvalidOperationIsReturned) {
    MockBuffer buffer{};
    auto data = 1;

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL);
    auto retVal = clEnqueueReadBuffer(
        pCommandQueue,
        &buffer,
        false,
        0,
        sizeof(data),
        &data,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(ClEnqueueReadBufferTests, GivenNullCommandQueueWhenReadingBufferThenInvalidCommandQueueErrorIsReturned) {
    auto data = 1;
    auto retVal = clEnqueueReadBuffer(
        nullptr,
        nullptr,
        false,
        0,
        sizeof(data),
        &data,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

class EnqueueReadBufferFlagsTest : public ApiFixture<>,
                                   public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  protected:
    void SetUp() override {
        ApiFixture::setUp();
        bufferFlags = GetParam();

        unsigned int bufferSize = 16;
        pHostMem = new unsigned char[bufferSize];
        memset(pHostMem, 0xaa, bufferSize);

        buffer = clCreateBuffer(
            pContext,
            bufferFlags,
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
        ApiFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    cl_mem_flags bufferFlags = 0;
    unsigned char *pHostMem;
    cl_mem buffer;
};

typedef EnqueueReadBufferFlagsTest EnqueueReadReadBufferTests;

TEST_P(EnqueueReadReadBufferTests, GivenNoReadFlagsWhenReadingBufferThenInvalidOperationErrorIsReturned) {
    auto data = 1;
    cl_event eventReturned = nullptr;
    retVal = clEnqueueReadBuffer(
        pCommandQueue,
        buffer,
        CL_TRUE,
        0,
        sizeof(data),
        &data,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    clReleaseEvent(eventReturned);
}

static cl_mem_flags readBufferFlags[] = {
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_SUITE_P(
    EnqueueReadBufferFlagsTests_Create,
    EnqueueReadReadBufferTests,
    testing::ValuesIn(readBufferFlags));
} // namespace ULT

class EnqueueReadBufferTest : public ApiTests {
  public:
    EnqueueReadBufferTest() {}

  protected:
    cl_mem buffer = nullptr;
    cl_int retVal = CL_SUCCESS;
    unsigned char *pHostMem = nullptr;
    unsigned int bufferSize = 0;

    void SetUp() override {
        ApiTests::SetUp();

        bufferSize = 16;

        pHostMem = new unsigned char[bufferSize];
        memset(pHostMem, 0xaa, bufferSize);

        buffer = clCreateBuffer(
            pContext,
            CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
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
        ApiTests::TearDown();
    }
};
TEST_F(EnqueueReadBufferTest, GivenSvmPtrWhenReadingBufferThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto data = clSVMAlloc(pContext, CL_MEM_READ_WRITE, bufferSize, 64);
        auto retVal = clEnqueueReadBuffer(pCommandQueue, buffer, CL_TRUE, bufferSize, 0, data, 0, nullptr, nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        clSVMFree(pContext, data);
    }
}
