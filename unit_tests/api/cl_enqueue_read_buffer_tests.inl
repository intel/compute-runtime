/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device/device.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/device/device_info.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueReadBufferTests;

namespace ULT {

TEST_F(clEnqueueReadBufferTests, GivenNullCommandQueueWhenReadingBufferThenInvalidCommandQueueErrorIsReturned) {
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

class EnqueueReadBufferFlagsTest : public ApiFixture,
                                   public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
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

static cl_mem_flags read_buffer_flags[] = {
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    EnqueueReadBufferFlagsTests_Create,
    EnqueueReadReadBufferTests,
    testing::ValuesIn(read_buffer_flags));
} // namespace ULT

class EnqueueReadBufferTest : public api_tests {
  public:
    EnqueueReadBufferTest() {}

  protected:
    cl_mem buffer = nullptr;
    cl_int retVal = CL_SUCCESS;
    unsigned char *pHostMem = nullptr;
    unsigned int bufferSize = 0;

    void SetUp() override {
        api_tests::SetUp();

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
        api_tests::TearDown();
    }
};
TEST_F(EnqueueReadBufferTest, GivenSvmPtrWhenReadingBufferThenSuccessIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(testedRootDeviceIndex)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto data = clSVMAlloc(pContext, CL_MEM_READ_WRITE, bufferSize, 64);
        auto retVal = clEnqueueReadBuffer(pCommandQueue, buffer, CL_TRUE, bufferSize, 0, data, 0, nullptr, nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        clSVMFree(pContext, data);
    }
}
