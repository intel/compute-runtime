/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/global_environment.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/helpers/kernel_filename_helper.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_source_level_debugger.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "opencl/test/unit_test/program/program_tests.h"
#include "test.h"

#include "compiler_options.h"
#include "gmock/gmock.h"

#include <algorithm>
#include <memory>
#include <string>

using namespace NEO;

TEST_F(ProgramTests, givenDeafultProgramObjectWhenKernelDebugEnabledIsQueriedThenFalseIsReturned) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false, pDevice);
    EXPECT_FALSE(program.isKernelDebugEnabled());
}

TEST_F(ProgramTests, givenProgramObjectWhenEnableKernelDebugIsCalledThenProgramHasKernelDebugEnabled) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false, pDevice);
    program.enableKernelDebug();
    EXPECT_TRUE(program.isKernelDebugEnabled());
}

class ProgramWithKernelDebuggingTest : public ProgramSimpleFixture,
                                       public ::testing::Test {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
        device = pClDevice;
        if (!pDevice->getHardwareInfo().capabilityTable.debuggerSupported) {
            GTEST_SKIP();
        }

        std::string filename;
        std::string kernelOption(CompilerOptions::debugKernelEnable);
        KernelFilenameHelper::getKernelFilenameFromInternalOption(kernelOption, filename);

        kbHelper = std::make_unique<KernelBinaryHelper>(filename, false);
        CreateProgramWithSource(
            pContext,
            &device,
            "copybuffer.cl");
        mockProgram = reinterpret_cast<MockProgram *>(pProgram);
        pProgram->enableKernelDebug();
    }

    void TearDown() override {
        ProgramSimpleFixture::TearDown();
    }
    cl_device_id device;
    std::unique_ptr<KernelBinaryHelper> kbHelper;
    MockProgram *mockProgram = nullptr;
};

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDebugFlag) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        std::string receivedInternalOptions;

        auto debugVars = NEO::getFclDebugVars();
        debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
        gEnvironment->fclPushDebugVars(debugVars);

        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(CompilerOptions::contains(receivedInternalOptions, CompilerOptions::debugKernelEnable)) << receivedInternalOptions;
        gEnvironment->fclPopDebugVars();
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDashGFlag) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-g"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsCompiledThenOptionsIncludeClOptDisableFlag) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {

        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->isOptDisabled = true;
        pDevice->executionEnvironment->debugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr(CompilerOptions::optDisable.data()));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenOptionsStartsWithDashSFilename) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->sourceCodeFilename = "debugFileName";
        pDevice->executionEnvironment->debugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->compile(1, &device, nullptr,
                                          0, nullptr, nullptr,
                                          nullptr,
                                          nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::StartsWith("-s debugFileName"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenInternalOptionsIncludeDebugFlag) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        std::string receivedInternalOptions;

        auto debugVars = NEO::getFclDebugVars();
        debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
        gEnvironment->fclPushDebugVars(debugVars);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(CompilerOptions::contains(receivedInternalOptions, CompilerOptions::debugKernelEnable)) << receivedInternalOptions;
        gEnvironment->fclPopDebugVars();
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsIncludeDashGFlag) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-g"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsBuiltThenOptionsIncludeClOptDisableFlag) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {

        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->isOptDisabled = true;
        pDevice->executionEnvironment->debugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr(CompilerOptions::optDisable.data()));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsStartsWithDashSFilename) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {

        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        sourceLevelDebugger->sourceCodeFilename = "debugFileName";
        pDevice->executionEnvironment->debugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_THAT(pProgram->getOptions(), ::testing::StartsWith("-s debugFileName"));
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsLinkedThenKernelDebugOptionsAreAppended) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {

        MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
        pDevice->executionEnvironment->debugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->compile(1, &device, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto program = std::unique_ptr<GMockProgram>(new GMockProgram(*pContext->getDevice(0)->getExecutionEnvironment(), pContext, false, &pContext->getDevice(0)->getDevice()));
        program->enableKernelDebug();

        EXPECT_CALL(*program, appendKernelDebugOptions()).Times(1);

        cl_program clProgramToLink = pProgram;
        retVal = program->link(1, &device, nullptr, 1, &clProgramToLink, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenDebuggerIsNotifiedWithKernelDebugData) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {

        GMockSourceLevelDebugger *sourceLevelDebugger = new GMockSourceLevelDebugger(nullptr);
        ON_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
        ON_CALL(*sourceLevelDebugger, isOptimizationDisabled()).WillByDefault(::testing::Return(false));

        EXPECT_CALL(*sourceLevelDebugger, isOptimizationDisabled()).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifyKernelDebugData(::testing::_)).Times(1);

        sourceLevelDebugger->setActive(true);
        pDevice->executionEnvironment->debugger.reset(sourceLevelDebugger);

        cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsLinkedThenDebuggerIsNotifiedWithKernelDebugData) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {

        GMockSourceLevelDebugger *sourceLevelDebugger = new GMockSourceLevelDebugger(nullptr);
        ON_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
        ON_CALL(*sourceLevelDebugger, isOptimizationDisabled()).WillByDefault(::testing::Return(false));

        EXPECT_CALL(*sourceLevelDebugger, isOptimizationDisabled()).Times(2);
        EXPECT_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifyKernelDebugData(::testing::_)).Times(1);

        sourceLevelDebugger->setActive(true);
        pDevice->executionEnvironment->debugger.reset(sourceLevelDebugger);

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
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        retVal = pProgram->build(1, &device, CompilerOptions::debugKernelEnable, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto kernelInfo = pProgram->getKernelInfo("CopyBuffer");
        EXPECT_NE(0u, kernelInfo->patchInfo.pAllocateSystemThreadSurface->PerThreadSystemThreadSurfaceSize);
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenKernelDebugEnabledWhenProgramIsBuiltThenDebugDataIsStored) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);

        auto debugData = mockProgram->getDebugData();
        EXPECT_NE(nullptr, debugData);
        EXPECT_NE(0u, mockProgram->getDebugDataSize());
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenProgramWithKernelDebugEnabledWhenProcessDebugDataIsCalledThenKernelInfosAreFilledWithDebugData) {
    if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
        retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        pProgram->processDebugData();

        auto kernelInfo = pProgram->getKernelInfo("CopyBuffer");

        EXPECT_NE(0u, kernelInfo->debugData.vIsaSize);
        EXPECT_NE(nullptr, kernelInfo->debugData.vIsa);
    }
}
