/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/test/common/device_binary_format/elf/elf_tests_data.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/kernel_filename_helper.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_debug_program.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_tests.h"

#include "program_debug_data.h"

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

class ZebinFallbackToPatchtokensLegacyDebugger : public ProgramTests {
  public:
    void SetUp() override {
        ProgramTests::SetUp();
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, mockRootDeviceIndex));
        const auto &hwInfo = device->getHardwareInfo();
        zebin.elfHeader->machine = hwInfo.platform.eProductFamily;
    }
    void TearDown() override {
        ProgramTests::TearDown();
    }
    ZebinTestData::ValidEmptyProgram<> zebin;
    std::unique_ptr<MockClDevice> device;
};

HWTEST_F(ZebinFallbackToPatchtokensLegacyDebugger, WhenCreatingProgramFromNonBuiltinZeBinaryWithSpirvDataIncludedAndLegacyDebuggerAttachedThenSuccessIsReturnedAndRebuildFromPTIsRequired) {
    if (sizeof(void *) != 8U) {
        GTEST_SKIP();
    }
    const uint8_t mockSpvData[0x10]{0};
    zebin.appendSection(Zebin::Elf::SHT_ZEBIN_SPIRV, Zebin::Elf::SectionNames::spv, mockSpvData);

    std::unique_ptr<MockProgram> program;
    device->executionEnvironment->rootDeviceEnvironments[mockRootDeviceIndex]->debugger.reset(new MockActiveSourceLevelDebugger);
    ASSERT_NE(nullptr, device->getSourceLevelDebugger());

    program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    auto retVal = program->createProgramFromBinary(zebin.storage.data(), zebin.storage.size(), *device.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(program->isCreatedFromBinary);

    EXPECT_TRUE(program->requiresRebuild);
    EXPECT_FALSE(CompilerOptions::contains(program->options, CompilerOptions::enableZebin));
}

HWTEST_F(ZebinFallbackToPatchtokensLegacyDebugger, WhenCreatingProgramFromNonBuiltinZeBinaryWithoutSpirvDataIncludedAndLegacyDebuggerAttachedThenErrorIsReturned) {
    if (sizeof(void *) != 8U) {
        GTEST_SKIP();
    }
    std::unique_ptr<MockProgram> program;
    device->executionEnvironment->rootDeviceEnvironments[mockRootDeviceIndex]->debugger.reset(new MockActiveSourceLevelDebugger);
    ASSERT_NE(nullptr, device->getSourceLevelDebugger());

    program = std::make_unique<MockProgram>(toClDeviceVector(*device));

    ASSERT_EQ(0u, program->irBinarySize);
    auto retVal = program->createProgramFromBinary(zebin.storage.data(), zebin.storage.size(), *device.get());
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

class ProgramWithKernelDebuggingFixture {
  public:
    void setUp() {
        pDevice = static_cast<MockDevice *>(&mockContext.getDevice(0)->getDevice());

        if (!pDevice->getHardwareInfo().capabilityTable.debuggerSupported) {
            GTEST_SKIP();
        }

        std::string filename;
        std::string kernelOption(CompilerOptions::debugKernelEnable);
        KernelFilenameHelper::getKernelFilenameFromInternalOption(kernelOption, filename);

        program = std::make_unique<MockDebugProgram>(mockContext.getDevices());
    }

    void tearDown() {}

    MockUnrestrictiveContext mockContext;
    std::unique_ptr<MockDebugProgram> program = nullptr;
    MockDevice *pDevice = nullptr;
};

using ProgramWithKernelDebuggingTest = Test<ProgramWithKernelDebuggingFixture>;

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDebugFlag) {
    cl_int retVal = program->compile(program->getDevices(), nullptr,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(CompilerOptions::contains(program->compilerInterface->buildInternalOptions, CompilerOptions::debugKernelEnable));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenInternalOptionsIncludeDashGFlag) {
    cl_int retVal = program->compile(program->getDevices(), nullptr,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(hasSubstr(program->getOptions(), "-g"));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsCompiledThenOptionsIncludeClOptDisableFlag) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = true;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = program->compile(program->getDevices(), nullptr,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(program->getOptions(), CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, GivenDebugVarDebuggerOptDisableZeroWhenOptDisableIsTrueFromDebuggerThenOptDisableIsNotAdded) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.DebuggerOptDisable.set(0);

    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = true;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = program->compile(program->getDevices(), nullptr,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(hasSubstr(program->getOptions(), CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, GivenDebugVarDebuggerOptDisableOneWhenOptDisableIsFalseFromDebuggerThenOptDisableIsAdded) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.DebuggerOptDisable.set(1);

    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = false;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = program->compile(program->getDevices(), nullptr,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(program->getOptions(), CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledThenOptionsStartsWithDashSFilename) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = program->compile(program->getDevices(), nullptr,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(startsWith(program->getOptions(), "-s \"debugFileName\""));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsCompiledWithCmCOptionThenDashSFilenameIsNotPrepended) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    char options[] = "-cmc -cl-opt-disable";
    cl_int retVal = program->compile(program->getDevices(), options,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(startsWith(program->getOptions(), "-s debugFileName"));
    EXPECT_TRUE(hasSubstr(program->getOptions(), CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenInternalOptionsIncludeDebugFlag) {
    cl_int retVal = program->build(program->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(CompilerOptions::contains(program->compilerInterface->buildInternalOptions, CompilerOptions::debugKernelEnable));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsIncludeDashGFlag) {
    cl_int retVal = program->build(program->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(program->getOptions(), "-g"));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugAndOptDisabledWhenProgramIsBuiltThenOptionsIncludeClOptDisableFlag) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->isOptDisabled = true;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = program->build(program->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(program->getOptions(), CompilerOptions::optDisable.data()));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenOptionsStartsWithDashSFilename) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    cl_int retVal = program->build(program->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(startsWith(program->getOptions(), "-s \"debugFileName\""));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltWithCmCOptionThenDashSFilenameIsNotPrepended) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    sourceLevelDebugger->sourceCodeFilename = "debugFileName";
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    char options[] = "-cmc -cl-opt-disable";
    cl_int retVal = program->build(program->getDevices(), options, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(startsWith(program->getOptions(), "-s debugFileName"));
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsLinkedThenKernelDebugOptionsAreAppended) {
    MockActiveSourceLevelDebugger *sourceLevelDebugger = new MockActiveSourceLevelDebugger;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);

    program->compilerInterface->output.debugData.size = 0;
    program->compilerInterface->output.debugData.mem = nullptr;
    cl_int retVal = program->compile(program->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_program clProgramToLink = program.get();
    auto &devices = program->getDevices();

    auto newProgram = std::unique_ptr<MockProgramAppendKernelDebugOptions>(new MockProgramAppendKernelDebugOptions(&mockContext, false, mockContext.getDevices()));
    newProgram->enableKernelDebug();
    retVal = newProgram->link(devices, nullptr, 1, &clProgramToLink);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<unsigned int>(mockContext.getRootDeviceIndices().size()), newProgram->appendKernelDebugOptionsCalled);
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsBuiltThenDebuggerIsNotifiedWithKernelDebugData) {
    const size_t rootDeviceIndicesSize = mockContext.getRootDeviceIndices().size();
    std::vector<MockSourceLevelDebugger *> sourceLevelDebugger(rootDeviceIndicesSize, nullptr);
    size_t i = 0;

    for (auto &rootDeviceIndex : mockContext.getRootDeviceIndices()) {
        sourceLevelDebugger[i] = new MockSourceLevelDebugger(nullptr);
        sourceLevelDebugger[i]->setActive(true);
        pDevice->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->debugger.reset(sourceLevelDebugger[i]);
        i++;
    }

    cl_int retVal = program->build(program->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &el : sourceLevelDebugger) {
        EXPECT_EQ(1u, el->isOptimizationDisabledCalled);
        EXPECT_EQ(false, el->isOptimizationDisabledResult);

        EXPECT_EQ(1u, el->notifySourceCodeCalled);
        EXPECT_EQ(false, el->notifySourceCodeResult);

        EXPECT_EQ(1u, el->notifyKernelDebugDataCalled);
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenEnabledKernelDebugWhenProgramIsLinkedThenDebuggerIsNotifiedWithKernelDebugData) {
    const size_t rootDeviceIndicesSize = mockContext.getRootDeviceIndices().size();
    std::vector<MockSourceLevelDebugger *> sourceLevelDebugger(rootDeviceIndicesSize, nullptr);
    size_t i = 0;

    for (auto &rootDeviceIndex : mockContext.getRootDeviceIndices()) {
        sourceLevelDebugger[i] = new MockSourceLevelDebugger(nullptr);
        sourceLevelDebugger[i]->setActive(true);
        pDevice->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->debugger.reset(sourceLevelDebugger[i]);
        i++;
    }

    cl_int retVal = program->compile(program->getDevices(), nullptr,
                                     0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_program clprogram = program.get();
    retVal = program->link(program->getDevices(), nullptr,
                           1, &clprogram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &el : sourceLevelDebugger) {
        EXPECT_EQ(2u, el->isOptimizationDisabledCalled);
        EXPECT_EQ(false, el->isOptimizationDisabledResult);

        EXPECT_EQ(1u, el->notifySourceCodeCalled);
        EXPECT_EQ(false, el->notifySourceCodeResult);

        EXPECT_EQ(1u, el->notifyKernelDebugDataCalled);
    }
}

TEST_F(ProgramWithKernelDebuggingTest, givenGtpinInitializedWhenCreatingProgramFromBinaryThenDebugDataIsAvailable) {
    bool gtpinInitializedBackup = NEO::isGTPinInitialized;
    NEO::isGTPinInitialized = true;
    auto retVal = program->build(program->getDevices(), CompilerOptions::debugKernelEnable.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(program->wasDebuggerNotified);

    NEO::isGTPinInitialized = gtpinInitializedBackup;
}

TEST_F(ProgramWithKernelDebuggingTest, givenGtpinNotInitializedWhenCreatingProgramFromBinaryThenDebugDataINullptr) {
    bool gtpinInitializedBackup = NEO::isGTPinInitialized;
    NEO::isGTPinInitialized = false;
    program->kernelDebugEnabled = false;
    auto retVal = program->build(program->getDevices(), CompilerOptions::debugKernelEnable.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(program->wasDebuggerNotified);

    NEO::isGTPinInitialized = gtpinInitializedBackup;
}

TEST_F(ProgramWithKernelDebuggingTest, givenKernelDebugEnabledWhenProgramIsBuiltThenDebugDataIsStored) {
    auto retVal = program->build(program->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto debugData = program->getDebugData(pDevice->getRootDeviceIndex());
    EXPECT_NE(nullptr, debugData);
    EXPECT_NE(0u, program->getDebugDataSize(pDevice->getRootDeviceIndex()));
}

TEST_F(ProgramWithKernelDebuggingTest, givenProgramWithKernelDebugEnabledWhenProcessDebugDataIsCalledThenKernelInfosAreFilledWithDebugData) {
    iOpenCL::SProgramDebugDataHeaderIGC debugDataHeader{};
    debugDataHeader.NumberOfKernels = 1u;

    char mockKernelName[] = "CopyBuffer";
    constexpr size_t mockKernelDebugDataSize = 0x10;
    PatchTokenBinary::SKernelDebugDataHeaderIGC mockKernelDebugHeader{};
    mockKernelDebugHeader.KernelNameSize = sizeof(mockKernelName);
    mockKernelDebugHeader.SizeVisaDbgInBytes = mockKernelDebugDataSize;

    char mockKerneDebugData[mockKernelDebugDataSize];
    memset(mockKerneDebugData, '\x01', mockKernelDebugDataSize);

    KernelInfo *mockKernelInfo = new KernelInfo{};
    mockKernelInfo->kernelDescriptor.kernelMetadata.kernelName = "CopyBuffer";
    program->addKernelInfo(mockKernelInfo, pDevice->getRootDeviceIndex());

    constexpr size_t mockDebugDataSize = sizeof(iOpenCL::SProgramDebugDataHeaderIGC) + sizeof(PatchTokenBinary::KernelFromPatchtokens) + sizeof(mockKernelName) + mockKernelDebugDataSize;

    char *mockDebugData = new char[mockDebugDataSize];
    auto dataPtr = mockDebugData;

    memcpy_s(dataPtr, mockDebugDataSize, &debugDataHeader, sizeof(iOpenCL::SProgramDebugDataHeaderIGC));
    dataPtr = ptrOffset(dataPtr, sizeof(iOpenCL::SProgramDebugDataHeaderIGC));
    memcpy_s(dataPtr, mockDebugDataSize, &mockKernelDebugHeader, sizeof(PatchTokenBinary::SKernelDebugDataHeaderIGC));
    dataPtr = ptrOffset(dataPtr, sizeof(PatchTokenBinary::SKernelDebugDataHeaderIGC));
    memcpy_s(dataPtr, mockDebugDataSize, &mockKernelName, sizeof(mockKernelName));
    dataPtr = ptrOffset(dataPtr, sizeof(mockKernelName));
    memcpy_s(dataPtr, mockDebugDataSize, mockKerneDebugData, mockKernelDebugDataSize);
    program->buildInfos[pDevice->getRootDeviceIndex()].debugData.reset(mockDebugData);

    program->processDebugData(pDevice->getRootDeviceIndex());
    auto receivedKernelInfo = program->getKernelInfo("CopyBuffer", pDevice->getRootDeviceIndex());

    EXPECT_NE(0u, receivedKernelInfo->debugData.vIsaSize);
    EXPECT_NE(nullptr, receivedKernelInfo->debugData.vIsa);
}

TEST_F(ProgramWithKernelDebuggingTest, givenProgramWithNonZebinaryFormatAndKernelDebugEnabledWhenProgramIsBuiltThenProcessDebugDataIsCalledAndDebuggerNotified) {
    MockSourceLevelDebugger *sourceLevelDebugger = new MockSourceLevelDebugger;
    pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(sourceLevelDebugger);
    program->enableKernelDebug();

    auto mockElf = std::make_unique<MockElfBinaryPatchtokens<>>(pDevice->getHardwareInfo());
    auto mockElfSize = mockElf->storage.size();
    auto mockElfData = mockElf->storage.data();

    program->buildInfos[pDevice->getRootDeviceIndex()].unpackedDeviceBinarySize = mockElfSize;
    program->buildInfos[pDevice->getRootDeviceIndex()].unpackedDeviceBinary.reset(new char[mockElfSize]);
    memcpy_s(program->buildInfos[pDevice->getRootDeviceIndex()].unpackedDeviceBinary.get(), program->buildInfos[pDevice->getRootDeviceIndex()].unpackedDeviceBinarySize,
             mockElfData, mockElfSize);

    KernelInfo *mockKernelInfo = new KernelInfo{};
    mockKernelInfo->kernelDescriptor.kernelMetadata.kernelName = "CopyBuffer";
    program->addKernelInfo(mockKernelInfo, pDevice->getRootDeviceIndex());

    auto counter = 0u;
    for (const auto &device : program->getDevices()) {
        program->notifyDebuggerWithDebugData(device);

        EXPECT_FALSE(program->wasCreateDebugZebinCalled);
        EXPECT_TRUE(program->wasProcessDebugDataCalled);
        EXPECT_EQ(++counter, sourceLevelDebugger->notifyKernelDebugDataCalled);
    }
}
