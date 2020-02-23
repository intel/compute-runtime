/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tracing/tracing_api.h"
#include "opencl/source/tracing/tracing_notify.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct IntelTracingMtTest : public api_tests {
  public:
    IntelTracingMtTest() : started(false), count(0) {}

    void SetUp() override {
        api_tests::SetUp();
    }

    void TearDown() override {
        api_tests::TearDown();
    }

  protected:
    static void threadBody(int iterationCount, IntelTracingMtTest *test) {
        test->vthreadBody(iterationCount);
    }

    virtual void vthreadBody(int iterationCount) {
        cl_int status = CL_SUCCESS;
        cl_platform_id platform = nullptr;
        const uint32_t maxStrSize = 1024;
        char buffer[maxStrSize] = {0};

        while (!started) {
        }

        for (int i = 0; i < iterationCount; ++i) {
            HostSideTracing::AtomicBackoff backoff;

            status = clGetDeviceInfo(devices[testedRootDeviceIndex], CL_DEVICE_NAME, maxStrSize, buffer, nullptr);
            EXPECT_EQ(CL_SUCCESS, status);
            backoff.pause();

            status = clGetDeviceInfo(devices[testedRootDeviceIndex], CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr);
            EXPECT_EQ(CL_SUCCESS, status);
            backoff.pause();

            status = clGetPlatformInfo(platform, CL_PLATFORM_VERSION, maxStrSize, buffer, nullptr);
            EXPECT_EQ(CL_SUCCESS, status);
            backoff.pause();

            status = clGetPlatformInfo(platform, CL_PLATFORM_NAME, maxStrSize, buffer, nullptr);
            EXPECT_EQ(CL_SUCCESS, status);
            backoff.pause();
        }
    }

    static void callback(cl_function_id fid, cl_callback_data *callbackData, void *userData) {
        ASSERT_NE(nullptr, userData);
        IntelTracingMtTest *base = (IntelTracingMtTest *)userData;
        base->vcallback(fid, callbackData, nullptr);
    }

    virtual void vcallback(cl_function_id fid, cl_callback_data *callbackData, void *userData) {
        if (fid == CL_FUNCTION_clGetDeviceInfo ||
            fid == CL_FUNCTION_clGetPlatformInfo) {
            ++count;
        }
    }

  protected:
    cl_tracing_handle handle = nullptr;
    cl_int status = CL_SUCCESS;

    std::atomic<bool> started;
    std::atomic<int> count;
};

TEST_F(IntelTracingMtTest, SafeTracingFromMultipleThreads) {
    status = clCreateTracingHandleINTEL(devices[testedRootDeviceIndex], callback, this, &handle);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clSetTracingPointINTEL(handle, CL_FUNCTION_clGetDeviceInfo, CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clSetTracingPointINTEL(handle, CL_FUNCTION_clGetPlatformInfo, CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clEnableTracingINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);

    int numThreads = 4;
    int iterationCount = 1024;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread(threadBody, iterationCount, this));
    }

    started = true;

    for (auto &thread : threads) {
        thread.join();
    }

    status = clDisableTracingINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);

    status = clDestroyTracingHandleINTEL(handle);
    EXPECT_EQ(CL_SUCCESS, status);

    int callsPerIteration = 4;
    int callbacksPerCall = 2;
    EXPECT_EQ(numThreads * iterationCount * callsPerIteration * callbacksPerCall, count);
}

} // namespace ULT