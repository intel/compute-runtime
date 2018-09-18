/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/kernel/kernel.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hash.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/surface.h"
#include "program_tests.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/program/program_from_binary.h"
#include "unit_tests/program/program_with_source.h"
#include "test.h"
#include <memory>
#include <vector>
#include <map>
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace OCLRT;

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
    MockDevice d(*platformDevices[0]);
    MockProgram pm(*d.getExecutionEnvironment());
    struct KernelMock : Kernel {
        KernelMock(Program *p, KernelInfo &ki, Device &d)
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

#include "runtime/helpers/options.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/mocks/mock_program.h"

#include <vector>

namespace OCLRT {

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
        device = pPlatform->getDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();
        CommandQueueHwFixture::SetUp(pPlatform->getDevice(0), 0);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
        CompilerInterface::shutdown();
    }
    cl_device_id device;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ProgramNonUniformTest, ExecuteKernelNonUniform21) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        CreateProgramFromBinary<MockProgram>(pContext, &device, "kernel_data_param");
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
        CreateProgramFromBinary<MockProgram>(pContext, &device, "kernel_data_param");
        auto mockProgram = (MockProgram *)pProgram;
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
    CreateProgramFromBinary<MockProgram>(pContext, &device, "kernel_data_param");
    auto mockProgram = (MockProgram *)pProgram;
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
} // namespace OCLRT
