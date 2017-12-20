/*
 * Copyright (c) 2017, Intel Corporation
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

using namespace OCLRT;

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

TEST_F(clSetMemObjectDestructorCallbackMtTests, bufferDestructorCallback_mtsafe) {
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
