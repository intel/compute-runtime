/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/mem_obj/pipe.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

using namespace NEO;

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

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
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

TEST_F(PipeTest, givenPipeWhenEnqueueWriteForUnmapIsCalledThenReturnError) {
    int errCode = CL_SUCCESS;
    std::unique_ptr<Pipe> pipe(Pipe::create(&context, CL_MEM_READ_ONLY, 1, 20, nullptr, errCode));
    ASSERT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, errCode);

    CommandQueue cmdQ;
    errCode = clEnqueueUnmapMemObject(&cmdQ, pipe.get(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, errCode);
}

TEST_F(PipeTest, givenPipeWithDifferentCpuAndGpuAddressesWhenSetArgPipeThenUseGpuAddress) {
    int errCode = CL_SUCCESS;

    auto pipe = Pipe::create(&context, CL_MEM_READ_ONLY, 1, 20, nullptr, errCode);

    ASSERT_NE(nullptr, pipe);
    EXPECT_EQ(CL_SUCCESS, errCode);

    EXPECT_EQ(21u, *reinterpret_cast<unsigned int *>(pipe->getCpuAddress()));
    uint64_t gpuAddress = 0x12345;
    auto pipeAllocation = pipe->getGraphicsAllocation();
    pipeAllocation->setCpuPtrAndGpuAddress(pipeAllocation->getUnderlyingBuffer(), gpuAddress);
    EXPECT_NE(reinterpret_cast<uint64_t>(pipeAllocation->getUnderlyingBuffer()), pipeAllocation->getGpuAddress());
    uint64_t valueToPatch;
    pipe->setPipeArg(&valueToPatch, sizeof(valueToPatch));
    EXPECT_EQ(valueToPatch, pipeAllocation->getGpuAddressToPatch());

    delete pipe;
}

using MultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(MultiRootDeviceTests, pipeGraphicsAllocationHasCorrectRootDeviceIndex) {
    int errCode = CL_SUCCESS;

    std::unique_ptr<Pipe> pipe(Pipe::create(context.get(), CL_MEM_READ_ONLY, 1, 20, nullptr, errCode));
    EXPECT_EQ(CL_SUCCESS, errCode);
    ASSERT_NE(nullptr, pipe.get());
    auto graphicsAllocation = pipe->getGraphicsAllocation();
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(expectedRootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
}