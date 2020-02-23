/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "opencl/test/unit_test/program/program_with_source.h"
#include "test.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "program_tests.h"

#include <map>
#include <memory>
#include <vector>

using namespace NEO;

class MyMockProgram : public MockProgram {
  public:
    MyMockProgram() : MockProgram(*new ExecutionEnvironment()), executionEnvironment(&this->peekExecutionEnvironment()) {}

  private:
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

TEST(ProgramNonUniform, UpdateAllowNonUniform) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions(nullptr);
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform12) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL1.2");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform20) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.0");
    pm.updateNonUniformFlag();
    EXPECT_TRUE(pm.getAllowNonUniform());
    EXPECT_EQ(20u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform21) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.1");
    pm.updateNonUniformFlag();
    EXPECT_TRUE(pm.getAllowNonUniform());
    EXPECT_EQ(21u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform20UniformFlag) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.0 -cl-uniform-work-group-size");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(20u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform21UniformFlag) {
    MyMockProgram pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.1 -cl-uniform-work-group-size");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(21u, pm.getProgramOptionVersion());
}

TEST(KernelNonUniform, GetAllowNonUniformFlag) {
    KernelInfo ki;
    MockClDevice d{new MockDevice};
    MockProgram pm(*d.getExecutionEnvironment());
    struct KernelMock : Kernel {
        KernelMock(Program *p, KernelInfo &ki, ClDevice &d)
            : Kernel(p, ki, d) {
        }
    };

    KernelMock k{&pm, ki, d};
    pm.setAllowNonUniform(false);
    EXPECT_FALSE(k.getAllowNonUniform());
    pm.setAllowNonUniform(true);
    EXPECT_TRUE(k.getAllowNonUniform());
    pm.setAllowNonUniform(false);
    EXPECT_FALSE(k.getAllowNonUniform());
}

TEST(ProgramNonUniform, UpdateAllowNonUniformOutcomeUniformFlag) {
    ExecutionEnvironment executionEnvironment;
    MockProgram pm(executionEnvironment);
    MockProgram pm1(executionEnvironment);
    MockProgram pm2(executionEnvironment);
    const MockProgram *inputPrograms[] = {&pm1, &pm2};
    cl_uint numInputPrograms = 2;

    pm1.setAllowNonUniform(false);
    pm2.setAllowNonUniform(false);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(pm.getAllowNonUniform());

    pm1.setAllowNonUniform(false);
    pm2.setAllowNonUniform(true);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(pm.getAllowNonUniform());

    pm1.setAllowNonUniform(true);
    pm2.setAllowNonUniform(false);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(pm.getAllowNonUniform());

    pm1.setAllowNonUniform(true);
    pm2.setAllowNonUniform(true);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_TRUE(pm.getAllowNonUniform());
}

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <vector>

namespace NEO {

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
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ProgramNonUniformTest, ExecuteKernelNonUniform21) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        CreateProgramFromBinary(pContext, &device, "kernel_data_param");
        auto mockProgram = (MockProgram *)pProgram;
        ASSERT_NE(nullptr, mockProgram);

        mockProgram->setBuildOptions("-cl-std=CL2.1");
        retVal = mockProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size");
        EXPECT_NE(nullptr, pKernelInfo);

        // create a kernel
        auto pKernel = Kernel::create<MockKernel>(mockProgram, *pKernelInfo, &retVal);
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
}

TEST_F(ProgramNonUniformTest, ExecuteKernelNonUniform20) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.0") != std::string::npos) {
        CreateProgramFromBinary(pContext, &device, "kernel_data_param");
        auto mockProgram = pProgram;
        ASSERT_NE(nullptr, mockProgram);

        mockProgram->setBuildOptions("-cl-std=CL2.0");
        retVal = mockProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size");
        EXPECT_NE(nullptr, pKernelInfo);

        // create a kernel
        auto pKernel = Kernel::create<MockKernel>(mockProgram, *pKernelInfo, &retVal);
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
        EXPECT_EQ(CL_SUCCESS, retVal);

        delete pKernel;
    }
}

TEST_F(ProgramNonUniformTest, ExecuteKernelNonUniform12) {
    CreateProgramFromBinary(pContext, &device, "kernel_data_param");
    auto mockProgram = pProgram;
    ASSERT_NE(nullptr, mockProgram);

    mockProgram->setBuildOptions("-cl-std=CL1.2");
    retVal = mockProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size");
    EXPECT_NE(nullptr, pKernelInfo);

    // create a kernel
    auto pKernel = Kernel::create<MockKernel>(mockProgram, *pKernelInfo, &retVal);
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
} // namespace NEO
