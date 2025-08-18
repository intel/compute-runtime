/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct DebuggerClFixture
    : public ContextFixture,
      public CommandQueueHwFixture {

    void setUp() {
        hardwareInfo = *NEO::defaultHwInfo.get();
        auto executionEnvironment = MockClDevice::prepareExecutionEnvironment(&hardwareInfo, rootDeviceIndex);
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
        device = MockDevice::createWithExecutionEnvironment<MockDevice>(&hardwareInfo, executionEnvironment, rootDeviceIndex);
        ASSERT_NE(nullptr, device);
        clExecutionEnvironment = static_cast<MockClExecutionEnvironment *>(device->getExecutionEnvironment());
        clDevice = new MockClDevice{device};
        ASSERT_NE(nullptr, clDevice);

        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initDebuggerL0(device);
        device->getL0Debugger()->initialize();
        this->osContext = device->getDefaultEngine().osContext;

        cl_device_id deviceId = clDevice;
        ContextFixture::setUp(1, &deviceId);
        CommandQueueHwFixture::setUp(clDevice, 0);
    }

    void tearDown() {
        ContextFixture::tearDown();
        CommandQueueHwFixture::tearDown();
        delete clDevice;
        clDevice = nullptr;
        device = nullptr;
    }

    MockDevice *device = nullptr;
    MockClDevice *clDevice = nullptr;
    HardwareInfo hardwareInfo = {};
    OsContext *osContext = nullptr;
    const uint32_t rootDeviceIndex = 0u;
    MockClExecutionEnvironment *clExecutionEnvironment = nullptr;
};

using DebuggerCmdQTest = Test<DebuggerClFixture>;

struct DebuggerCmdQTestWithMockCsr : public DebuggerCmdQTest {
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCsr<FamilyType>>();

        DebuggerCmdQTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        DebuggerCmdQTest::TearDown();
    }
};

HWTEST_F(DebuggerCmdQTest, GivenDebuggingEnabledWhenCommandQueueIsCreatedAndReleasedThenDebuggerL0IsNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    debuggerL0Hw->commandQueueCreatedCount = 0;
    debuggerL0Hw->commandQueueDestroyedCount = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, clDevice, 0);

    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);
    cl_int retVal = 0;
    releaseQueue(mockCmdQ, retVal);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);
}

HWTEST_F(DebuggerCmdQTest, GivenDebuggingEnabledWhenInternalCommandQueueIsCreatedAndReleasedThenDebuggerL0IsNotNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    debuggerL0Hw->commandQueueCreatedCount = 0;
    debuggerL0Hw->commandQueueDestroyedCount = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, clDevice, 0, true);

    EXPECT_EQ(0u, debuggerL0Hw->commandQueueCreatedCount);
    cl_int retVal = 0;
    releaseQueue(mockCmdQ, retVal);
    EXPECT_EQ(0u, debuggerL0Hw->commandQueueDestroyedCount);
}

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;
HWTEST2_TEMPLATED_F(DebuggerCmdQTestWithMockCsr, GivenDebuggingEnabledWhenEnqueueingKernelThenDebugSurfaceIsResident, Gen12Plus) {
    auto mockCSR = static_cast<MockCsr<FamilyType> *>(&device->getUltCommandStreamReceiver<FamilyType>());

    MockKernelWithInternals mockKernelWithInternals(*clDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;
    pCmdQ->initializeGpgpu();
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 0, nullptr, nullptr);

    EXPECT_TRUE(mockCSR->isMadeResident(mockCSR->getDebugSurfaceAllocation()));
}

struct DebuggerZebinProgramTest : public Test<DebuggerClFixture> {
    void setUp() {
        Test<DebuggerClFixture>::setUp();
    }
    void tearDown() {
        Test<DebuggerClFixture>::tearDown();
    }

    MockProgram *createProgram() {
        auto zebin = ZebinTestData::ValidEmptyProgram<>();

        auto program = new MockProgram(toClDeviceVector(*clDevice));

        program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = zebin.storage.size();
        program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
        memcpy_s(program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize,
                 zebin.storage.data(), zebin.storage.size());

        auto kernelInfo = new KernelInfo();
        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;
        auto mockAlloc = new MockGraphicsAllocation(rootDeviceIndex, nullptr, 0x10000);
        mockAlloc->gpuAddress = 0x80000000;
        kernelInfo->kernelAllocation = mockAlloc;

        program->addKernelInfo(kernelInfo, rootDeviceIndex);
        auto globalSurface = new MockGraphicsAllocation(rootDeviceIndex, nullptr, 0x10000);
        globalSurface->gpuAddress = 0x80020000;
        program->setGlobalSurface(globalSurface);
        return program;
    }

    void releaseProgram(MockProgram *&program) {
        delete program->getKernelInfoArray(rootDeviceIndex)[0]->kernelAllocation;
        delete program->getKernelInfoArray(rootDeviceIndex)[0];
        program->getKernelInfoArray(rootDeviceIndex).clear();
        delete program->getGlobalSurfaceGA(rootDeviceIndex);
        program->setGlobalSurface(nullptr);
        delete program;
        program = nullptr;
    }
};

HWTEST_F(DebuggerZebinProgramTest, GivenDebuggingEnabledWhenProgramNotifiesModuleCreationThenDebuggerL0IsNotifiedWithAllData) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    EXPECT_EQ(0u, debuggerL0Hw->registerElfCount);
    EXPECT_EQ(0u, debuggerL0Hw->segmentCountWithAttachedModuleHandle);
    EXPECT_EQ(nullptr, debuggerL0Hw->notifyModuleLoadAllocationsCapturedDevice);
    EXPECT_EQ(0u, debuggerL0Hw->notifyModuleCreateCount);
    debuggerL0Hw->elfHandleToReturn = 5;
    debuggerL0Hw->moduleHandleToReturn = 6;

    auto program = createProgram();

    program->notifyModuleCreate();

    EXPECT_EQ(1u, debuggerL0Hw->registerElfCount);
    EXPECT_EQ(2u, debuggerL0Hw->segmentCountWithAttachedModuleHandle);
    EXPECT_EQ(&clDevice->getDevice(), debuggerL0Hw->notifyModuleLoadAllocationsCapturedDevice);
    EXPECT_EQ(1u, debuggerL0Hw->notifyModuleCreateCount);

    EXPECT_EQ(5u, program->debuggerInfos[rootDeviceIndex].debugElfHandle);
    EXPECT_EQ(6u, program->debuggerInfos[rootDeviceIndex].debugModuleHandle);
    EXPECT_EQ(program->getKernelInfo(static_cast<size_t>(0u), rootDeviceIndex)->getGraphicsAllocation()->getGpuAddress(), program->debuggerInfos[rootDeviceIndex].moduleLoadAddress);

    releaseProgram(program);
}

HWTEST_F(DebuggerZebinProgramTest, GivenDebuggingEnabledAndNonZebinWhenProgramNotifiesModuleCreationThenDebuggerL0IsNotNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    debuggerL0Hw->elfHandleToReturn = 5;
    debuggerL0Hw->moduleHandleToReturn = 6;

    auto program = createProgram();

    PatchTokensTestData::ValidEmptyProgram programTokens;

    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = programTokens.storage.size();
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset(new char[programTokens.storage.size()]);
    memcpy_s(program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize,
             programTokens.storage.data(), programTokens.storage.size());

    program->notifyModuleCreate();

    EXPECT_EQ(0u, debuggerL0Hw->registerElfCount);
    EXPECT_EQ(0u, debuggerL0Hw->segmentCountWithAttachedModuleHandle);
    EXPECT_EQ(nullptr, debuggerL0Hw->notifyModuleLoadAllocationsCapturedDevice);
    EXPECT_EQ(0u, debuggerL0Hw->notifyModuleCreateCount);

    EXPECT_EQ(0u, program->debuggerInfos[rootDeviceIndex].debugElfHandle);
    EXPECT_EQ(0u, program->debuggerInfos[rootDeviceIndex].debugModuleHandle);
    EXPECT_EQ(0u, program->debuggerInfos[rootDeviceIndex].moduleLoadAddress);

    releaseProgram(program);
}

HWTEST_F(DebuggerZebinProgramTest, GivenDebuggingEnabledWhenProgramNotifiesModuleDestroyThenDebuggerL0IsNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    auto program = createProgram();

    program->debuggerInfos[rootDeviceIndex].debugElfHandle = 5;
    program->debuggerInfos[rootDeviceIndex].debugModuleHandle = 6;

    program->notifyModuleDestroy();

    EXPECT_EQ(6u, debuggerL0Hw->removedZebinModuleHandle);
    EXPECT_EQ(1u, debuggerL0Hw->notifyModuleDestroyCount);

    delete program;
}

HWTEST_F(DebuggerZebinProgramTest, GivenProgramWhenBuildingThenNotifyModuleCreateIsCalled) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    auto mockCompilerInterface = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->compilerInterface.reset(mockCompilerInterface);

    auto zebin = ZebinTestData::ValidEmptyProgram < is32bit ? NEO::Elf::EI_CLASS_32 : NEO::Elf::EI_CLASS_64 > ();
    auto program = new MockProgram(toClDeviceVector(*clDevice));

    auto copyHwInfo = device->getHardwareInfo();
    auto &compilerProductHelper = device->getCompilerProductHelper();
    compilerProductHelper.adjustHwInfoForIgc(copyHwInfo);

    zebin.elfHeader->machine = copyHwInfo.platform.eProductFamily;

    mockCompilerInterface->output.intermediateRepresentation.size = zebin.storage.size();
    mockCompilerInterface->output.intermediateRepresentation.mem.reset(new char[zebin.storage.size()]);

    memcpy_s(mockCompilerInterface->output.intermediateRepresentation.mem.get(), mockCompilerInterface->output.intermediateRepresentation.size,
             zebin.storage.data(), zebin.storage.size());

    program->createdFrom = Program::CreatedFrom::binary;
    program->irBinary = std::make_unique<char[]>(16);
    program->irBinarySize = 16;

    cl_int retVal = program->build(program->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, debuggerL0Hw->notifyModuleCreateCount);

    delete program;
}

HWTEST_F(DebuggerZebinProgramTest, GivenProgramWhenLinkingThenNotifyModuleCreateIsCalled) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    auto mockCompilerInterface = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->compilerInterface.reset(mockCompilerInterface);

    auto zebin = ZebinTestData::ValidEmptyProgram < is32bit ? NEO::Elf::EI_CLASS_32 : NEO::Elf::EI_CLASS_64 > ();
    auto program = new MockProgram(toClDeviceVector(*clDevice));

    auto copyHwInfo = device->getHardwareInfo();
    auto &compilerProductHelper = device->getCompilerProductHelper();
    compilerProductHelper.adjustHwInfoForIgc(copyHwInfo);

    zebin.elfHeader->machine = copyHwInfo.platform.eProductFamily;

    mockCompilerInterface->output.intermediateRepresentation.size = zebin.storage.size();
    mockCompilerInterface->output.intermediateRepresentation.mem.reset(new char[zebin.storage.size()]);

    memcpy_s(mockCompilerInterface->output.intermediateRepresentation.mem.get(), mockCompilerInterface->output.intermediateRepresentation.size,
             zebin.storage.data(), zebin.storage.size());

    program->createdFrom = Program::CreatedFrom::binary;
    program->irBinary = std::make_unique<char[]>(16);
    program->irBinarySize = 16;

    cl_int retVal = program->compile(program->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_program clProgramToLink = program;
    auto &devices = program->getDevices();

    auto newProgram = new MockProgram(toClDeviceVector(*clDevice));

    retVal = newProgram->link(devices, nullptr, 1, &clProgramToLink);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, debuggerL0Hw->notifyModuleCreateCount);

    delete newProgram;
    delete program;
}

HWTEST_F(DebuggerZebinProgramTest, GivenProgramWhenDestroyedThenNotifiesDebugger) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    auto program = createProgram();
    releaseProgram(program);
    EXPECT_EQ(1u, debuggerL0Hw->notifyModuleDestroyCount);
}