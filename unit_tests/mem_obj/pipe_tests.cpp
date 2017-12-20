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

#include "runtime/command_queue/command_queue.h"
#include "runtime/mem_obj/pipe.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "test.h"

using namespace OCLRT;

//Tests for pipes

class PipeTest : public DeviceFixture, public ::testing::Test, public MemoryManagementFixture {
  public:
    PipeTest() {}

  protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    size_t size;
};

TEST_F(PipeTest, CreatePipe) {
    int errCode = CL_SUCCESS;

    auto pipe = Pipe::create(&context, CL_MEM_READ_ONLY, 1, 20, nullptr, errCode);

    EXPECT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, errCode);

    delete pipe;
}

TEST_F(PipeTest, PipeCheckReservedHeaderSizeAddition) {
    int errCode = CL_SUCCESS;

    auto pipe = Pipe::create(&context, CL_MEM_READ_ONLY, 1, 20, nullptr, errCode);

    ASSERT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ((1 * (20 + 1)) + Pipe::intelPipeHeaderReservedSpace, pipe->getSize());

    delete pipe;
}

TEST_F(PipeTest, PipeCheckHeaderinitialization) {
    int errCode = CL_SUCCESS;

    auto pipe = Pipe::create(&context, CL_MEM_READ_ONLY, 1, 20, nullptr, errCode);

    ASSERT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, errCode);

    EXPECT_EQ(21u, *reinterpret_cast<unsigned int *>(pipe->getCpuAddress()));

    delete pipe;
}

TEST_F(PipeTest, FailedAllocationInjection) {
    InjectedFunction method = [this](size_t failureIndex) {
        auto retVal = CL_INVALID_VALUE;
        auto pipe = Pipe::create(&context, CL_MEM_READ_ONLY, 1, 20, nullptr, retVal);

        if (nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, pipe);
            delete pipe;
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, pipe);
        }
    };
    injectFailures(method);
}

TEST_F(PipeTest, givenPipeWhenUnmapIsCalledThenReturnError) {
    int errCode = CL_SUCCESS;
    auto pipe = Pipe::create(&context, CL_MEM_READ_ONLY, 1, 20, nullptr, errCode);
    ASSERT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, errCode);
    auto cmdQ = CommandQueue::create(&context, context.getDevice(0), 0, errCode);
    EXPECT_EQ(CL_SUCCESS, errCode);

    errCode = pipe->unmapObj(cmdQ, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, errCode);

    delete pipe;
    delete cmdQ;
}
