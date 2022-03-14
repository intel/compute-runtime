/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "opencl/test/unit_test/program/program_with_source.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"
#include "program_tests.h"

#include <map>
#include <memory>
#include <vector>

using namespace NEO;

class MyMockProgram : public MockProgram {
  public:
    MyMockProgram() : MockProgram(toClDeviceVector(*(new MockClDevice(new MockDevice())))), device(this->clDevices[0]) {}

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

class ProgramNonUniformTest : public ContextFixture,
                              public PlatformFixture,
                              public ProgramFixture,
                              public CommandQueueHwFixture,
                              public testing::Test {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

  protected:
    ProgramNonUniformTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp();
        device = pPlatform->getClDevice(0);
        rootDeviceIndex = pPlatform->getClDevice(0)->getRootDeviceIndex();
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();
        CommandQueueHwFixture::SetUp(pPlatform->getClDevice(0), 0);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
    }
    cl_device_id device;
    uint32_t rootDeviceIndex;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ProgramNonUniformTest, GivenCl21WhenExecutingKernelWithNonUniformThenEnqueueSucceeds) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    CreateProgramFromBinary(pContext, pContext->getDevices(), "kernel_data_param");
    auto mockProgram = pProgram;
    ASSERT_NE(nullptr, mockProgram);

    mockProgram->setBuildOptions("-cl-std=CL2.1");
    retVal = mockProgram->build(
        mockProgram->getDevices(),
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size", rootDeviceIndex);
    EXPECT_NE(nullptr, pKernelInfo);

    // create a kernel
    auto pKernel = Kernel::create<MockKernel>(mockProgram,
                                              *pKernelInfo,
                                              *pPlatform->getClDevice(0),
                                              &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

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

    delete pKernel;
}

TEST_F(ProgramNonUniformTest, GivenCl20WhenExecutingKernelWithNonUniformThenEnqueueSucceeds) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    CreateProgramFromBinary(pContext, pContext->getDevices(), "kernel_data_param");
    auto mockProgram = pProgram;
    ASSERT_NE(nullptr, mockProgram);

    mockProgram->setBuildOptions("-cl-std=CL2.0");
    retVal = mockProgram->build(
        mockProgram->getDevices(),
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size", rootDeviceIndex);
    EXPECT_NE(nullptr, pKernelInfo);

    // create a kernel
    auto pKernel = Kernel::create<MockKernel>(mockProgram,
                                              *pKernelInfo,
                                              *pPlatform->getClDevice(0),
                                              &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

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

    delete pKernel;
}

TEST_F(ProgramNonUniformTest, GivenCl12WhenExecutingKernelWithNonUniformThenInvalidWorkGroupSizeIsReturned) {
    CreateProgramFromBinary(pContext, pContext->getDevices(), "kernel_data_param");
    auto mockProgram = pProgram;
    ASSERT_NE(nullptr, mockProgram);

    mockProgram->setBuildOptions("-cl-std=CL1.2");
    retVal = mockProgram->build(
        mockProgram->getDevices(),
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size", rootDeviceIndex);
    EXPECT_NE(nullptr, pKernelInfo);

    // create a kernel
    auto pKernel = Kernel::create<MockKernel>(mockProgram,
                                              *pKernelInfo,
                                              *pPlatform->getClDevice(0),
                                              &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

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

    delete pKernel;
}
