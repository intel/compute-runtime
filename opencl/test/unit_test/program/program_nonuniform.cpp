/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class MyMockProgram : public MockProgram {
  public:
    MyMockProgram() : MockProgram(toClDeviceVector(*(new MockClDevice(new MockDevice())))), device(this->clDevices[0]) {}
    ~MyMockProgram() override {
        clDevices.clear();
    }

  private:
    std::unique_ptr<ClDevice> device;
};

TEST(ProgramNonUniform, GivenNoBuildOptionsWhenUpdatingAllowNonUniformThenNonUniformNotAllowed) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions(nullptr);
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, GivenBuildOptionsCl12WhenUpdatingAllowNonUniformThenNonUniformNotAllowed) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL1.2");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, GivenBuildOptionsCl20WhenUpdatingAllowNonUniformThenNonUniformAllowed) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.0");
    pm.updateNonUniformFlag();
    EXPECT_TRUE(pm.getAllowNonUniform());
    EXPECT_EQ(20u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, GivenBuildOptionsCl21WhenUpdatingAllowNonUniformThenNonUniformAllowed) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.1");
    pm.updateNonUniformFlag();
    EXPECT_TRUE(pm.getAllowNonUniform());
    EXPECT_EQ(21u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, GivenBuildOptionsCl20AndUniformFlagWhenUpdatingAllowNonUniformThenNonUniformNotAllowed) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.0 -cl-uniform-work-group-size");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(20u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, GivenBuildOptionsCl21AndUniformFlagWhenUpdatingAllowNonUniformThenNonUniformNotAllowed) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.1 -cl-uniform-work-group-size");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(21u, pm.getProgramOptionVersion());
}

TEST(KernelNonUniform, WhenSettingAllowNonUniformThenGettingAllowNonUniformReturnsCorrectValue) {
    KernelInfo kernelInfo;
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    struct KernelMock : Kernel {
        KernelMock(Program *program, KernelInfo &kernelInfos, ClDevice &clDeviceArg)
            : Kernel(program, kernelInfos, clDeviceArg) {
        }
    };
    KernelMock k{&program, kernelInfo, device};
    program.setAllowNonUniform(false);
    EXPECT_FALSE(k.getAllowNonUniform());
    program.setAllowNonUniform(true);
    EXPECT_TRUE(k.getAllowNonUniform());
    program.setAllowNonUniform(false);
    EXPECT_FALSE(k.getAllowNonUniform());
}

TEST(ProgramNonUniform, WhenSettingAllowNonUniformThenGettingAllowNonUniformReturnsCorrectValue) {
    MockClDevice device{new MockDevice()};
    auto deviceVector = toClDeviceVector(device);
    MockProgram program(deviceVector);
    MockProgram program1(deviceVector);
    MockProgram program2(deviceVector);
    const MockProgram *inputPrograms[] = {&program1, &program2};
    cl_uint numInputPrograms = 2;

    program1.setAllowNonUniform(false);
    program2.setAllowNonUniform(false);
    program.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(program.getAllowNonUniform());

    program1.setAllowNonUniform(false);
    program2.setAllowNonUniform(true);
    program.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(program.getAllowNonUniform());

    program1.setAllowNonUniform(true);
    program2.setAllowNonUniform(false);
    program.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(program.getAllowNonUniform());

    program1.setAllowNonUniform(true);
    program2.setAllowNonUniform(true);
    program.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_TRUE(program.getAllowNonUniform());
}

class ProgramNonUniformTest : public ClDeviceFixture,
                              public CommandQueueHwFixture,
                              public testing::Test {

  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueHwFixture::setUp(pClDevice, 0);
    }

    void TearDown() override {
        CommandQueueHwFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ProgramNonUniformTest, GivenNonUniformAllowedWhenExecutingKernelWithNonUniformThenEnqueueSucceeds) {
    MockKernelWithInternals mockKernelWithInternals(*context, MockKernelWithInternalsConfig{.addDefaultArgs = true});
    mockKernelWithInternals.mockProgram->setAllowNonUniform(true);
    auto pKernel = mockKernelWithInternals.mockKernel;

    size_t globalWorkSize[3] = {12, 12, 12};
    size_t localWorkSize[3] = {11, 12, 1};

    retVal = pCmdQ->enqueueKernel(
        pKernel,
        3,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramNonUniformTest, GivenNonUniformNotAllowedWhenExecutingKernelWithNonUniformThenInvalidWorkGroupSizeIsReturned) {
    MockKernelWithInternals mockKernelWithInternals(*context, MockKernelWithInternalsConfig{.addDefaultArgs = true});
    mockKernelWithInternals.mockProgram->setAllowNonUniform(false);
    auto pKernel = mockKernelWithInternals.mockKernel;

    size_t globalWorkSize[3] = {12, 12, 12};
    size_t localWorkSize[3] = {11, 12, 12};

    retVal = pCmdQ->enqueueKernel(
        pKernel,
        3,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}
