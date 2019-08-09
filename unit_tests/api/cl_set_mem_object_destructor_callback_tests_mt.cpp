/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateBufferTests;

namespace ULT {

static int cbInvoked = 0;
void CL_CALLBACK destructorCallBackMt(cl_mem memObj, void *userData) {
    cbInvoked++;
}

struct clSetMemObjectDestructorCallbackMtTests : public api_fixture,
                                                 public ::testing::Test {

    void SetUp() override {
        api_fixture::SetUp();
        cbInvoked = 0;
    }

    void TearDown() override {
        api_fixture::TearDown();
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
