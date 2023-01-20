/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateBufferTests;

namespace ULT {

static int cbInvoked = 0;
void CL_CALLBACK destructorCallBackMt(cl_mem memObj, void *userData) {
    cbInvoked++;
}

struct clSetMemObjectDestructorCallbackMtTests : public ApiFixture<>,
                                                 public ::testing::Test {

    void SetUp() override {
        ApiFixture::setUp();
        cbInvoked = 0;
    }

    void TearDown() override {
        ApiFixture::tearDown();
    }

    static void setMemCallbackThreadFunc(cl_mem buf) {
        auto ret = clSetMemObjectDestructorCallback(buf, destructorCallBackMt, nullptr);
        EXPECT_EQ(CL_SUCCESS, ret);
    }
};

TEST_F(clSetMemObjectDestructorCallbackMtTests, GivenMultipleThreadsWhenSettingDestructorCallbackThenCallbackWasInvokedForEachThread) {
    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 42, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    std::thread t1(clSetMemObjectDestructorCallbackMtTests::setMemCallbackThreadFunc, buffer);
    std::thread t2(clSetMemObjectDestructorCallbackMtTests::setMemCallbackThreadFunc, buffer);
    retVal = clSetMemObjectDestructorCallback(buffer, destructorCallBackMt, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    t1.join();
    t2.join();

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3, cbInvoked);
}
} // namespace ULT
