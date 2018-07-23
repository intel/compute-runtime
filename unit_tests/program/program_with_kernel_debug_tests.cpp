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
#include "unit_tests/helpers/kernel_filename_helper.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"
#include "unit_tests/program/program_tests.h"
#include "unit_tests/program/program_from_binary.h"
#include "test.h"
#include "gmock/gmock.h"
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

        std::string filename;
        std::string kernelOption(CompilerOptions::debugKernelEnable);
        KernelFilenameHelper::getKernelFilenameFromInternalOption(kernelOption, filename);

        kbHelper = new KernelBinaryHelper(filename, false);
        CreateProgramWithSource<MockProgram>(
            pContext,
            &device,
            "copybuffer.cl");
        mockProgram = reinterpret_cast<MockProgram *>(pProgram);
        pProgram->enableKernelDebug();
    }

    void TearDown() override {
        delete kbHelper;
        ProgramSimpleFixture::TearDown();
    }
    cl_device_id device;
    KernelBinaryHelper *kbHelper = nullptr;
    MockProgram *mockProgram = nullptr;
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

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDashGFlag) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-g"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsCompiledThenOptionsIncludeClOptDisableFlag) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {

        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->isOptDisabled = true;
        pDevice->executionEnvironment->sourceLevelDebugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-cl-opt-disable"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenOptionsStartsWithDashSFilename) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->sourceCodeFilename = "debugFileName";
        pDevice->executionEnvironment->sourceLevelDebugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::StartsWith("-s debugFileName"));
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

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsIncludeDashGFlag) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-g"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsBuiltThenOptionsIncludeClOptDisableFlag) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {

        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->isOptDisabled = true;
        pDevice->executionEnvironment->sourceLevelDebugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-cl-opt-disable"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsStartsWithDashSFilename) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {

        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->sourceCodeFilename = "debugFileName";
        pDevice->executionEnvironment->sourceLevelDebugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::StartsWith("-s debugFileName"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenDebuggerIsNotifiedWithKernelDebugData) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {

        GMockSourceLevelDebugger *sourceLevelDebugger = new GMockSourceLevelDebugger(nullptr);
        ON_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
        ON_CALL(*sourceLevelDebugger, isOptimizationDisabled()).WillByDefault(::testing::Return(false));

        EXPECT_CALL(*sourceLevelDebugger, isOptimizationDisabled()).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifyKernelDebugData(::testing::_)).Times(1);

        sourceLevelDebugger->setActive(true);
        pDevice->executionEnvironment->sourceLevelDebugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsLinkedThenDebuggerIsNotifiedWithKernelDebugData) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {

        GMockSourceLevelDebugger *sourceLevelDebugger = new GMockSourceLevelDebugger(nullptr);
        ON_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
        ON_CALL(*sourceLevelDebugger, isOptimizationDisabled()).WillByDefault(::testing::Return(false));

        EXPECT_CALL(*sourceLevelDebugger, isOptimizationDisabled()).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifyKernelDebugData(::testing::_)).Times(1);

        sourceLevelDebugger->setActive(true);
        pDevice->executionEnvironment->sourceLevelDebugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        cl_program program = pProgram;
        retVal = pProgram->link(1, &device, nullptr,
                                1, &program, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
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

TEST_F(ProgramWithKernelDebuggingTest, givenKernelDebugEnabledWhenProgramIsBuiltThenDebugDataIsStored) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);

        size_t debugDataSize = 0;
        auto debugData = mockProgram->getDebugDataBinary(debugDataSize);
        EXPECT_NE(nullptr, debugData);
        EXPECT_NE(0u, debugDataSize);
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenProgramWithKernelDebugEnabledWhenProcessDebugDataIsCalledThenKernelInfosAreFilledWithDebugData) {
    if (pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE) {
        retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        pProgram->processDebugData();

        auto kernelInfo = pProgram->getKernelInfo("CopyBuffer");

        EXPECT_NE(0u, kernelInfo->debugData.genIsaSize);
        EXPECT_NE(0u, kernelInfo->debugData.vIsaSize);

        EXPECT_NE(nullptr, kernelInfo->debugData.genIsa);
        EXPECT_NE(nullptr, kernelInfo->debugData.vIsa);
    }
}
