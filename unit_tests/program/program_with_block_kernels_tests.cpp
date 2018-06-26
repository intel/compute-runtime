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

#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/fixtures/run_kernel_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_program.h"

#include <vector>

namespace OCLRT {

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
        device = pPlatform->getDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();
    }

    void TearDown() override {
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
        CompilerInterface::shutdown();
    }
    cl_device_id device;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ProgramWithBlockKernelsTest, GivenKernelWithBlockKernelsWhenProgramIsBuildingThenKernelInfosHaveCorrectNames) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        CreateProgramFromBinary<MockProgram>(pContext, &device, "simple_block_kernel", "-cl-std=CL2.0");
        auto mockProgram = (MockProgram *)pProgram;
        ASSERT_NE(nullptr, mockProgram);

        retVal = mockProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto kernelInfo = mockProgram->Program::getKernelInfo("simple_block_kernel");
        EXPECT_NE(nullptr, kernelInfo);

        auto blockKernelInfo = mockProgram->Program::getKernelInfo("simple_block_kernel_dispatch_0");
        EXPECT_EQ(nullptr, blockKernelInfo);

        std::vector<const KernelInfo *> blockKernelInfos(mockProgram->getNumberOfBlocks());

        for (size_t i = 0; i < mockProgram->getNumberOfBlocks(); i++) {
            const KernelInfo *blockKernelInfo = mockProgram->getBlockKernelInfo(i);
            EXPECT_NE(nullptr, blockKernelInfo);
            blockKernelInfos[i] = blockKernelInfo;
        }

        bool blockKernelFound = false;
        for (size_t i = 0; i < mockProgram->getNumberOfBlocks(); i++) {
            if (blockKernelInfos[i]->name.find("simple_block_kernel_dispatch") != std::string::npos) {
                blockKernelFound = true;
                break;
            }
        }

        EXPECT_TRUE(blockKernelFound);

    } else {
        EXPECT_EQ(nullptr, pProgram);
    }
}

TEST_F(ProgramWithBlockKernelsTest, GivenKernelWithBlockKernelsWhenProgramIsLinkedThenBlockKernelsAreSeparated) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.0") != std::string::npos) {
        CreateProgramFromBinary<Program>(pContext, &device, "simple_block_kernel", "-cl-std=CL2.0");
        const char *buildOptions = "-cl-std=CL2.0";

        overwriteBuiltInBinaryName(
            pPlatform->getDevice(0),
            "simple_block_kernel", true);

        ASSERT_NE(nullptr, pProgram);

        EXPECT_EQ(CL_SUCCESS, retVal);
        Program *programLinked = new Program(pContext, false);
        cl_program program = pProgram;

        retVal = pProgram->compile(1, &device, buildOptions, 0, nullptr, nullptr, nullptr, nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = programLinked->link(1, &device, buildOptions, 1, &program, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        BlockKernelManager *blockManager = programLinked->getBlockKernelManager();

        EXPECT_NE(0u, blockManager->getCount());

        for (uint32_t i = 0; i < blockManager->getCount(); i++) {
            const KernelInfo *info = blockManager->getBlockKernelInfo(i);
            if (info->name.find("simple_block_kernel_dispatch") != std::string::npos) {
                break;
            }
        }
        restoreBuiltInBinaryName(nullptr);
        delete programLinked;
    } else {
        EXPECT_EQ(nullptr, pProgram);
    }
}
} // namespace OCLRT
