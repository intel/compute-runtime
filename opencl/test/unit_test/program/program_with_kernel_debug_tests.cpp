/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

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
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    EXPECT_FALSE(program.isKernelDebugEnabled());
}

TEST_F(ProgramTests, givenProgramObjectWhenEnableKernelDebugIsCalledThenProgramHasKernelDebugEnabled) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    program.enableKernelDebug();
    EXPECT_TRUE(program.isKernelDebugEnabled());
}

TEST(ProgramFromBinary, givenBinaryWithDebugDataWhenCreatingProgramFromBinaryThenDebugDataIsAvailable) {
    if (!defaultHwInfo->capabilityTable.debuggerSupported) {
        GTEST_SKIP();
    }
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "-cl-kernel-debug-enable_", ".bin");

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    program->enableKernelDebug();

    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize, *device);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, program->getDebugData());
    EXPECT_NE(0u, program->getDebugDataSize());
}

class ProgramWithKernelDebuggingTest : public ProgramFixture,
                                       public ::testing::Test {
  public:
    void SetUp() override {
        pDevice = static_cast<MockDevice *>(&mockContext.getDevice(0)->getDevice());

        if (!pDevice->getHardwareInfo().capabilityTable.debuggerSupported) {
            GTEST_SKIP();
        }

        std::string filename;
        std::string kernelOption(CompilerOptions::debugKernelEnable);
        KernelFilenameHelper::getKernelFilenameFromInternalOption(kernelOption, filename);

        kbHelper = std::make_unique<KernelBinaryHelper>(filename, false);
        CreateProgramWithSource(
            &mockContext,
            "copybuffer.cl");
        pProgram->enableKernelDebug();
    }

    void TearDown() override {
        ProgramFixture::TearDown();
    }
    std::unique_ptr<KernelBinaryHelper> kbHelper;
    MockUnrestrictiveContext mockContext;
    MockDevice *pDevice = nullptr;
};

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDebugFlag) {
    std::string receivedInternalOptions;

    auto debugVars = NEO::getFclDebugVars();
    debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(debugVars);

    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(CompilerOptions::contains(receivedInternalOptions, CompilerOptions::debugKernelEnable)) << receivedInternalOptions;
    gEnvironment->fclPopDebugVars();
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDashGFlag) {
    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-g"));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsCompiledThenOptionsIncludeClOptDisableFlag) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = true;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr(CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, GivenDebugVarDebuggerOptDisableZeroWhenOptDisableIsTrueFromDebuggerThenOptDisableIsNotAdded) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.DebuggerOptDisable.set(0);

    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = true;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::Not(::testing::HasSubstr(CompilerOptions::optDisable.data())));
}

TEST_F(ProgramWithKernelDebuggingTest, GivenDebugVarDebuggerOptDisableOneWhenOptDisableIsFalseFromDebuggerThenOptDisableIsAdded) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.DebuggerOptDisable.set(1);

    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = false;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr(CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenOptionsStartsWithDashSFilename) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::StartsWith("-s debugFileName"));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledWithCmCOptionThenDashSFilenameIsNotPrepended) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    char options[] = "-cmc -cl-opt-disable";
    cl_int retVal = pProgram->compile(pProgram->getDevices(), options,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::Not(::testing::StartsWith("-s debugFileName")));
    EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr(CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenInternalOptionsIncludeDebugFlag) {
    std::string receivedInternalOptions;

    auto debugVars = NEO::getFclDebugVars();
    debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(debugVars);

    cl_int retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(CompilerOptions::contains(receivedInternalOptions, CompilerOptions::debugKernelEnable)) << receivedInternalOptions;
    gEnvironment->fclPopDebugVars();
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsIncludeDashGFlag) {
    cl_int retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr("-g"));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsBuiltThenOptionsIncludeClOptDisableFlag) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = true;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::HasSubstr(CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsStartsWithDashSFilename) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::StartsWith("-s debugFileName"));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltWithCmCOptionThenDashSFilenameIsNotPrepended) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    char options[] = "-cmc -cl-opt-disable";
    cl_int retVal = pProgram->build(pProgram->getDevices(), options, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getOptions(), ::testing::Not(::testing::StartsWith("-s debugFileName")));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsLinkedThenKernelDebugOptionsAreAppended) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto program = std::unique_ptr<GMockProgram>(new GMockProgram(&mockContext, false, mockContext.getDevices()));
    program->enableKernelDebug();

    EXPECT_CALL(*program, appendKernelDebugOptions(::testing::_, ::testing::_)).Times(static_cast<int>(mockContext.getRootDeviceIndices().size()));

    cl_program clProgramToLink = pProgram;
    retVal = program->link(pProgram->getDevices(), nullptr, 1, &clProgramToLink);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenDebuggerIsNotifiedWithKernelDebugData) {
    for (auto &rootDeviceIndex : mockContext.getRootDeviceIndices()) {
        GMockSourceLevelDebugger *sourceLevelDebugger = new GMockSourceLevelDebugger(nullptr);
        ON_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
        ON_CALL(*sourceLevelDebugger, isOptimizationDisabled()).WillByDefault(::testing::Return(false));

        EXPECT_CALL(*sourceLevelDebugger, isOptimizationDisabled()).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifyKernelDebugData(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(1);

        sourceLevelDebugger->setActive(true);
        pDevice->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->debugger.reset(sourceLevelDebugger);
    }

    cl_int retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsLinkedThenDebuggerIsNotifiedWithKernelDebugData) {
    for (auto &rootDeviceIndex : mockContext.getRootDeviceIndices()) {
        GMockSourceLevelDebugger *sourceLevelDebugger = new GMockSourceLevelDebugger(nullptr);
        ON_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(false));
        ON_CALL(*sourceLevelDebugger, isOptimizationDisabled()).WillByDefault(::testing::Return(false));

        EXPECT_CALL(*sourceLevelDebugger, isOptimizationDisabled()).Times(2);
        EXPECT_CALL(*sourceLevelDebugger, notifySourceCode(::testing::_, ::testing::_, ::testing::_)).Times(1);
        EXPECT_CALL(*sourceLevelDebugger, notifyKernelDebugData(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(1);

        sourceLevelDebugger->setActive(true);
        pDevice->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->debugger.reset(sourceLevelDebugger);
    }

    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr,
                                      0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_program program = pProgram;
    retVal = pProgram->link(pProgram->getDevices(), nullptr,
                            1, &program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramWithKernelDebuggingTest, givenProgramWithKernelDebugEnabledWhenBuiltThenPatchTokenAllocateSipSurfaceHasSizeGreaterThanZero) {
    auto retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::debugKernelEnable.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernelInfo = pProgram->getKernelInfo("CopyBuffer", pDevice->getRootDeviceIndex());
    EXPECT_NE(0u, kernelInfo->kernelDescriptor.kernelAttributes.perThreadSystemThreadSurfaceSize);
}

TEST_F(ProgramWithKernelDebuggingTest, givenGtpinInitializedWhenCreatingProgramFromBinaryThenDebugDataIsAvailable) {
    bool gtpinInitializedBackup = NEO::isGTPinInitialized;
    NEO::isGTPinInitialized = true;
    auto retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::debugKernelEnable.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernelInfo = pProgram->getKernelInfo("CopyBuffer", pDevice->getRootDeviceIndex());
    EXPECT_NE(kernelInfo->debugData.vIsa, nullptr);
    EXPECT_NE(0u, kernelInfo->debugData.vIsaSize);

    NEO::isGTPinInitialized = gtpinInitializedBackup;
}

TEST_F(ProgramWithKernelDebuggingTest, givenGtpinNotInitializedWhenCreatingProgramFromBinaryThenDebugDataINullptr) {
    bool gtpinInitializedBackup = NEO::isGTPinInitialized;
    NEO::isGTPinInitialized = false;
    pProgram->kernelDebugEnabled = false;
    auto retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::debugKernelEnable.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernelInfo = pProgram->getKernelInfo("CopyBuffer", pDevice->getRootDeviceIndex());
    EXPECT_EQ(kernelInfo->debugData.vIsa, nullptr);
    EXPECT_EQ(0u, kernelInfo->debugData.vIsaSize);

    NEO::isGTPinInitialized = gtpinInitializedBackup;
}

TEST_F(ProgramWithKernelDebuggingTest, givenKernelDebugEnabledWhenProgramIsBuiltThenDebugDataIsStored) {
    auto retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto debugData = pProgram->getDebugData();
    EXPECT_NE(nullptr, debugData);
    EXPECT_NE(0u, pProgram->getDebugDataSize());
}

TEST_F(ProgramWithKernelDebuggingTest, givenProgramWithKernelDebugEnabledWhenProcessDebugDataIsCalledThenKernelInfosAreFilledWithDebugData) {
    auto retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram->processDebugData(pDevice->getRootDeviceIndex());

    auto kernelInfo = pProgram->getKernelInfo("CopyBuffer", pDevice->getRootDeviceIndex());

    EXPECT_NE(0u, kernelInfo->debugData.vIsaSize);
    EXPECT_NE(nullptr, kernelInfo->debugData.vIsa);
}
