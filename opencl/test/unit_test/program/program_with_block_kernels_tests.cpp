/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/program/block_kernel_manager.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/fixtures/run_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include <vector>

namespace NEO {

class ProgramWithBlockKernelsTest : public ContextFixture,
                                    public PlatformFixture,
                                    public ProgramFixture,
                                    public testing::Test {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

  protected:
    ProgramWithBlockKernelsTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp();
        device = pPlatform->getClDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();
        REQUIRE_OCL_21_OR_SKIP(pContext);
    }

    void TearDown() override {
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
    }
    cl_device_id device;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ProgramWithBlockKernelsTest, GivenKernelWithBlockKernelsWhenProgramIsBuildingThenKernelInfosHaveCorrectNames) {
    CreateProgramFromBinary(pContext, pContext->getDevices(), "simple_block_kernel", "-cl-std=CL2.0");
    auto mockProgram = pProgram;
    ASSERT_NE(nullptr, mockProgram);

    retVal = mockProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernelInfo = mockProgram->Program::getKernelInfo("simple_block_kernel");
    EXPECT_NE(nullptr, kernelInfo);

    auto blockKernelInfo = mockProgram->Program::getKernelInfo("simple_block_kernel_dispatch_0");
    EXPECT_EQ(nullptr, blockKernelInfo);

    std::vector<const KernelInfo *> blockKernelInfos(mockProgram->blockKernelManager->getCount());

    for (size_t i = 0; i < mockProgram->blockKernelManager->getCount(); i++) {
        const KernelInfo *blockKernelInfo = mockProgram->blockKernelManager->getBlockKernelInfo(i);
        EXPECT_NE(nullptr, blockKernelInfo);
        blockKernelInfos[i] = blockKernelInfo;
    }

    bool blockKernelFound = false;
    for (size_t i = 0; i < mockProgram->blockKernelManager->getCount(); i++) {
        if (blockKernelInfos[i]->kernelDescriptor.kernelMetadata.kernelName.find("simple_block_kernel_dispatch") != std::string::npos) {
            blockKernelFound = true;
            break;
        }
    }

    EXPECT_TRUE(blockKernelFound);
}

TEST_F(ProgramWithBlockKernelsTest, GivenKernelWithBlockKernelsWhenProgramIsLinkedThenBlockKernelsAreSeparated) {
    CreateProgramFromBinary(pContext, pContext->getDevices(), "simple_block_kernel", "-cl-std=CL2.0");
    const char *buildOptions = "-cl-std=CL2.0";

    overwriteBuiltInBinaryName("simple_block_kernel", true);

    ASSERT_NE(nullptr, pProgram);

    EXPECT_EQ(CL_SUCCESS, retVal);
    Program *programLinked = new Program(pContext, false, pContext->getDevices());
    cl_program program = pProgram;

    retVal = pProgram->compile(pProgram->getDevices(), buildOptions, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = programLinked->link(pProgram->getDevices(), buildOptions, 1, &program);
    EXPECT_EQ(CL_SUCCESS, retVal);

    BlockKernelManager *blockManager = programLinked->getBlockKernelManager();

    EXPECT_NE(0u, blockManager->getCount());

    for (uint32_t i = 0; i < blockManager->getCount(); i++) {
        const KernelInfo *info = blockManager->getBlockKernelInfo(i);
        if (info->kernelDescriptor.kernelMetadata.kernelName.find("simple_block_kernel_dispatch") != std::string::npos) {
            break;
        }
    }
    restoreBuiltInBinaryName();
    delete programLinked;
}

} // namespace NEO
