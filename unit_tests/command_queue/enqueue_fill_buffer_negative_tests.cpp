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

#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/command_queue/enqueue_fill_buffer_fixture.h"
#include "gtest/gtest.h"

using namespace OCLRT;

namespace ULT {

struct EnqueueFillBuffer : public EnqueueFillBufferFixture,
                           public ::testing::Test {
    typedef EnqueueFillBufferFixture BaseClass;

    void SetUp() override {
        BaseClass::SetUp();
    }

    void TearDown() override {
        BaseClass::TearDown();
    }
};

TEST_F(EnqueueFillBuffer, null_buffer) {
    cl_float pattern = 1.0f;
    auto retVal = clEnqueueFillBuffer(
        BaseClass::pCmdQ,
        nullptr,
        &pattern,
        sizeof(pattern),
        0,
        sizeof(pattern),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(EnqueueFillBuffer, null_pattern) {
    cl_float pattern = 1.0f;
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto retVal = clEnqueueFillBuffer(
        BaseClass::pCmdQ,
        buffer,
        nullptr,
        sizeof(pattern),
        0,
        sizeof(pattern),
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueFillBuffer, null_event_list) {
    cl_float pattern = 1.0f;

    auto retVal = clEnqueueFillBuffer(
        BaseClass::pCmdQ,
        buffer,
        &pattern,
        sizeof(pattern),
        0,
        sizeof(pattern),
        1,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(EnqueueFillBuffer, invalid_event_list_count) {
    cl_event eventList = (cl_event)ptrGarbage;
    cl_float pattern = 1.0f;

    auto retVal = clEnqueueFillBuffer(
        BaseClass::pCmdQ,
        buffer,
        &pattern,
        sizeof(pattern),
        0,
        sizeof(pattern),
        0,
        &eventList,
        nullptr);

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}
} // namespace ULT

namespace ULT {

struct InvalidPatternSize : public EnqueueFillBufferFixture,
                            public ::testing::TestWithParam<size_t> {
    typedef EnqueueFillBufferFixture BaseClass;

    InvalidPatternSize() {
    }

    void SetUp() override {
        BaseClass::SetUp();
        patternSize = GetParam();
        pattern = new char[patternSize];
    }

    void TearDown() override {
        delete[] pattern;
        BaseClass::TearDown();
    }

    size_t patternSize = 0;
    char *pattern = nullptr;
};

TEST_P(InvalidPatternSize, returns_CL_INVALID_VALUE) {
    auto retVal = clEnqueueFillBuffer(
        BaseClass::pCmdQ,
        buffer,
        &pattern,
        patternSize,
        0,
        patternSize,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

INSTANTIATE_TEST_CASE_P(EnqueueFillBuffer,
                        InvalidPatternSize,
                        ::testing::Values(0, 3, 5, 256, 512, 1024));
} // namespace ULT
