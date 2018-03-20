/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/compiler_interface/compiler_options.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/program/program_tests.h"
#include "unit_tests/program/program_from_binary.h"
#include "test.h"
#include "gmock/gmock-matchers.h"
#include <algorithm>
#include <memory>
#include <string>

using namespace OCLRT;

TEST_F(ProgramTests, givenDeafultProgramObjectWhenKernelDebugEnabledIsQueriedThenFalseIsReturned) {
    MockProgram program(pContext, false);
    EXPECT_FALSE(program.isKernelDebugEnabled());
}

TEST_F(ProgramTests, givenProgramObjectWhenEnableKernelDebugIsCalledThenProgramHasKernelDebugEnabled) {
    MockProgram program(pContext, false);
    program.enableKernelDebug();
    EXPECT_TRUE(program.isKernelDebugEnabled());
}

class ProgramWithKernelDebuggingTest : public ProgramSimpleFixture,
                                       public ::testing::Test {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
        device = pDevice;

        std::string fullName(CompilerOptions::debugKernelEnable);
        // remove leading spaces
        size_t position = fullName.find_first_not_of(" ");
        std::string filename(fullName, position);
        // replace space with underscore
        std::replace(filename.begin(), filename.end(), ' ', '_');

        kbHelper = new KernelBinaryHelper(filename, false);
        CreateProgramWithSource(
            pContext,
            &device,
            "copybuffer.cl");
        pProgram->enableKernelDebug();
    }

    void TearDown() override {
        delete kbHelper;
        ProgramSimpleFixture::TearDown();
    }
    cl_device_id device;
    KernelBinaryHelper *kbHelper = nullptr;
};

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDebugFlag) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        std::string receivedInternalOptions;

        auto debugVars = OCLRT::getFclDebugVars();
        debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
        gEnvironment->fclPushDebugVars(debugVars);

        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_THAT(receivedInternalOptions, ::testing::HasSubstr(CompilerOptions::debugKernelEnable));
        gEnvironment->fclPopDebugVars();
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenInternalOptionsIncludeDebugFlag) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        std::string receivedInternalOptions;

        auto debugVars = OCLRT::getFclDebugVars();
        debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
        gEnvironment->fclPushDebugVars(debugVars);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_THAT(receivedInternalOptions, ::testing::HasSubstr(CompilerOptions::debugKernelEnable));
        gEnvironment->fclPopDebugVars();
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenProgramWithKernelDebugEnabledWhenBuiltThenPatchTokenAllocateSipSurfaceHasSizeGreaterThanZero) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        retVal = pProgram->build(1, &device, CompilerOptions::debugKernelEnable, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto kernelInfo = pProgram->getKernelInfo("CopyBuffer");
        EXPECT_NE(0u, kernelInfo->patchInfo.pAllocateSystemThreadSurface->PerThreadSystemThreadSurfaceSize);
    }
}
