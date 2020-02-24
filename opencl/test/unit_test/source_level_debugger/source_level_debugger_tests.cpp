/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/platform/platform.h"
#include "opencl/source/program/kernel_info.h"
#include "opencl/source/source_level_debugger/source_level_debugger.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/libult/source_level_debugger_library.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_source_level_debugger.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace NEO;
using std::string;
using std::unique_ptr;

class DebuggerLibraryRestorer {
  public:
    DebuggerLibraryRestorer() {
        restoreActiveState = DebuggerLibrary::getDebuggerActive();
        restoreAvailableState = DebuggerLibrary::getLibraryAvailable();
    }
    ~DebuggerLibraryRestorer() {
        DebuggerLibrary::clearDebuggerLibraryInterceptor();
        DebuggerLibrary::setDebuggerActive(restoreActiveState);
        DebuggerLibrary::setLibraryAvailable(restoreAvailableState);
    }
    bool restoreActiveState = false;
    bool restoreAvailableState = false;
};

TEST(SourceLevelDebugger, givenPlatformWhenItIsCreatedThenSourceLevelDebuggerIsCreatedInExecutionEnvironment) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.debuggerSupported) {
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);
        auto executionEnvironment = new ExecutionEnvironment();
        MockPlatform platform(*executionEnvironment);
        platform.initializeWithNewDevices();

        EXPECT_NE(nullptr, executionEnvironment->debugger);
    }
}

TEST(SourceLevelDebugger, givenNoKernelDebuggerLibraryWhenSourceLevelDebuggerIsCreatedThenLibraryIsNotLoaded) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(false);

    MockSourceLevelDebugger debugger;
    EXPECT_EQ(nullptr, debugger.debuggerLibrary.get());
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryAvailableWhenSourceLevelDebuggerIsConstructedThenLibraryIsLoaded) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(true);

    MockSourceLevelDebugger debugger;
    EXPECT_NE(nullptr, debugger.debuggerLibrary.get());
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryAvailableWhenIsDebuggerActiveIsCalledThenFalseIsReturned) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(true);

    MockSourceLevelDebugger debugger;
    bool active = debugger.isDebuggerActive();
    EXPECT_FALSE(active);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenIsDebuggerActiveIsCalledThenTrueIsReturned) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);

    MockSourceLevelDebugger debugger;
    bool active = debugger.isDebuggerActive();
    EXPECT_TRUE(active);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotAvailableWhenIsDebuggerActiveIsCalledThenFalseIsReturned) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(false);

    MockSourceLevelDebugger debugger;
    bool active = debugger.isDebuggerActive();
    EXPECT_FALSE(active);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenNotifySourceCodeIsCalledThenDebuggerLibraryFunctionIsCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    GfxDbgSourceCode argOut;
    char fileName[] = "filename";
    argOut.sourceName = fileName;
    argOut.sourceNameMaxLen = sizeof(fileName);
    interceptor.sourceCodeArgOut = &argOut;

    const char source[] = "sourceCode";
    string file;
    debugger.notifySourceCode(source, sizeof(source), file);

    EXPECT_TRUE(interceptor.sourceCodeCalled);
    EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(MockSourceLevelDebugger::mockDeviceHandle)), interceptor.sourceCodeArgIn.hDevice);
    EXPECT_EQ(source, interceptor.sourceCodeArgIn.sourceCode);
    EXPECT_EQ(sizeof(source), interceptor.sourceCodeArgIn.sourceCodeSize);
    EXPECT_NE(nullptr, interceptor.sourceCodeArgIn.sourceName);
    EXPECT_NE(0u, interceptor.sourceCodeArgIn.sourceNameMaxLen);

    EXPECT_STREQ(fileName, file.c_str());
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenNotifySourceCodeIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(false);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.setActive(false);

    const char source[] = "sourceCode";
    string file;
    debugger.notifySourceCode(source, sizeof(source), file);
    EXPECT_FALSE(interceptor.sourceCodeCalled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenNotifyNewDeviceIsCalledThenDebuggerLibraryFunctionIsCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;
    debugger.notifyNewDevice(4);

    EXPECT_TRUE(interceptor.newDeviceCalled);
    EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(4u)), interceptor.newDeviceArgIn.dh);
    EXPECT_EQ(4u, debugger.deviceHandle);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenNotifyNewDeviceIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(false);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.setActive(false);
    debugger.notifyNewDevice(4);
    EXPECT_FALSE(interceptor.newDeviceCalled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenIsOptimizationDisabledIsCalledThenDebuggerLibraryFunctionIsCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;
    bool isOptDisabled = debugger.isOptimizationDisabled();
    EXPECT_FALSE(isOptDisabled);

    EXPECT_TRUE(interceptor.optionCalled);
    EXPECT_EQ(GfxDbgOptionNames::DBG_OPTION_IS_OPTIMIZATION_DISABLED, interceptor.optionArgIn.optionName);
    EXPECT_NE(nullptr, interceptor.optionArgIn.value);
    EXPECT_LT(0u, interceptor.optionArgIn.valueLen);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenIsOptimizationDisabledIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.setActive(false);
    bool isOptDisabled = debugger.isOptimizationDisabled();
    EXPECT_FALSE(isOptDisabled);
    EXPECT_FALSE(interceptor.optionCalled);
}

TEST(SourceLevelDebugger, givenActiveDebuggerWhenGetDebuggerOptionReturnsZeroThenIsOptimizationDisabledReturnsFalse) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    char value = '1';
    GfxDbgOption optionArgOut;
    interceptor.optionArgOut = &optionArgOut;
    interceptor.optionArgOut->value = &value;
    interceptor.optionArgOut->valueLen = sizeof(value);
    interceptor.optionRetVal = 0;

    MockSourceLevelDebugger debugger;
    bool isOptDisabled = debugger.isOptimizationDisabled();
    EXPECT_FALSE(isOptDisabled);
}

TEST(SourceLevelDebugger, givenActiveDebuggerAndOptDisabledWhenGetDebuggerOptionReturnsNonZeroAndOneInValueThenIsOptimizationDisabledReturnsTrue) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    char value[2] = {'1', 0};
    GfxDbgOption optionArgOut;
    interceptor.optionArgOut = &optionArgOut;
    interceptor.optionArgOut->value = value;
    interceptor.optionArgOut->valueLen = sizeof(value);
    interceptor.optionRetVal = 1;

    MockSourceLevelDebugger debugger;
    bool isOptDisabled = debugger.isOptimizationDisabled();
    EXPECT_TRUE(isOptDisabled);
}

TEST(SourceLevelDebugger, givenActiveDebuggerAndOptDisabledWhenGetDebuggerOptionReturnsNonZeroAndZeroInValueThenIsOptimizationDisabledReturnsFalse) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    char value = '0';
    GfxDbgOption optionArgOut;
    interceptor.optionArgOut = &optionArgOut;
    interceptor.optionArgOut->value = &value;
    interceptor.optionArgOut->valueLen = sizeof(value);
    interceptor.optionRetVal = 1;

    MockSourceLevelDebugger debugger;
    bool isOptDisabled = debugger.isOptimizationDisabled();
    EXPECT_FALSE(isOptDisabled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenNotifyKernelDebugDataIsCalledThenDebuggerLibraryFunctionIsCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;
    char isa[8];
    char dbgIsa[10];
    char visa[12];

    KernelInfo info;
    info.debugData.genIsa = dbgIsa;
    info.debugData.vIsa = visa;
    info.debugData.genIsaSize = sizeof(dbgIsa);
    info.debugData.vIsaSize = sizeof(visa);

    info.name = "debugKernel";

    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.KernelHeapSize = sizeof(isa);
    info.heapInfo.pKernelHeader = &kernelHeader;
    info.heapInfo.pKernelHeap = isa;

    debugger.notifyKernelDebugData(&info);

    EXPECT_TRUE(interceptor.kernelDebugDataCalled);

    EXPECT_EQ(static_cast<uint32_t>(IGFXDBG_CURRENT_VERSION), interceptor.kernelDebugDataArgIn.version);
    EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(MockSourceLevelDebugger::mockDeviceHandle)), interceptor.kernelDebugDataArgIn.hDevice);
    EXPECT_EQ(reinterpret_cast<GenRtProgramHandle>(0), interceptor.kernelDebugDataArgIn.hProgram);

    EXPECT_EQ(dbgIsa, interceptor.kernelDebugDataArgIn.dbgGenIsaBuffer);
    EXPECT_EQ(sizeof(dbgIsa), interceptor.kernelDebugDataArgIn.dbgGenIsaSize);
    EXPECT_EQ(visa, interceptor.kernelDebugDataArgIn.dbgVisaBuffer);
    EXPECT_EQ(sizeof(visa), interceptor.kernelDebugDataArgIn.dbgVisaSize);

    EXPECT_EQ(kernelHeader.KernelHeapSize, interceptor.kernelDebugDataArgIn.KernelBinSize);
    EXPECT_EQ(isa, interceptor.kernelDebugDataArgIn.kernelBinBuffer);
    EXPECT_STREQ(info.name.c_str(), interceptor.kernelDebugDataArgIn.kernelName);
}

TEST(SourceLevelDebugger, givenNoVisaWhenNotifyKernelDebugDataIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;
    char isa[8];
    char dbgIsa[10];

    KernelInfo info;
    info.debugData.genIsa = dbgIsa;
    info.debugData.vIsa = nullptr;
    info.debugData.genIsaSize = sizeof(dbgIsa);
    info.debugData.vIsaSize = 0;

    info.name = "debugKernel";

    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.KernelHeapSize = sizeof(isa);
    info.heapInfo.pKernelHeader = &kernelHeader;
    info.heapInfo.pKernelHeap = isa;

    debugger.notifyKernelDebugData(&info);
    EXPECT_FALSE(interceptor.kernelDebugDataCalled);
}

TEST(SourceLevelDebugger, givenNoGenIsaWhenNotifyKernelDebugDataIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;
    char isa[8];
    char visa[12];

    KernelInfo info;
    info.debugData.genIsa = nullptr;
    info.debugData.vIsa = visa;
    info.debugData.genIsaSize = 0;
    info.debugData.vIsaSize = sizeof(visa);

    info.name = "debugKernel";

    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.KernelHeapSize = sizeof(isa);
    info.heapInfo.pKernelHeader = &kernelHeader;
    info.heapInfo.pKernelHeap = isa;

    debugger.notifyKernelDebugData(&info);
    EXPECT_FALSE(interceptor.kernelDebugDataCalled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenNotifyKernelDebugDataIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(false);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.setActive(false);
    KernelInfo info;
    debugger.notifyKernelDebugData(&info);
    EXPECT_FALSE(interceptor.kernelDebugDataCalled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenInitializeIsCalledWithLocalMemoryUsageFalseThenDebuggerFunctionIsCalledWithCorrectArg) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.initialize(false);
    EXPECT_TRUE(interceptor.initCalled);
    EXPECT_FALSE(interceptor.targetCapsArgIn.supportsLocalMemory);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenInitializeReturnsErrorThenIsActiveIsSetToFalse) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    interceptor.initRetVal = IgfxdbgRetVal::IGFXDBG_FAILURE;
    debugger.initialize(false);
    EXPECT_TRUE(interceptor.initCalled);
    EXPECT_FALSE(debugger.isDebuggerActive());
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenInitializeIsCalledWithLocalMemoryUsageTrueThenDebuggerFunctionIsCalledWithCorrectArg) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.initialize(true);
    EXPECT_TRUE(interceptor.initCalled);
    EXPECT_TRUE(interceptor.targetCapsArgIn.supportsLocalMemory);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenInitializeIsCalledThenDebuggerFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(false);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.initialize(false);
    EXPECT_FALSE(interceptor.initCalled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenDeviceIsConstructedThenDebuggerIsInitialized) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.debuggerSupported) {
        DebuggerLibraryInterceptor interceptor;
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);
        DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

        unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        EXPECT_TRUE(interceptor.initCalled);
    }
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenDeviceImplIsCreatedThenDebuggerIsNotified) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.debuggerSupported) {
        DebuggerLibraryInterceptor interceptor;
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);
        DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

        unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        unique_ptr<MockClDevice> pClDevice(new MockClDevice{device.get()});
        EXPECT_TRUE(interceptor.newDeviceCalled);
        uint32_t deviceHandleExpected = device->getGpgpuCommandStreamReceiver().getOSInterface() != nullptr ? device->getGpgpuCommandStreamReceiver().getOSInterface()->getDeviceHandle() : 0;
        EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandleExpected)), interceptor.newDeviceArgIn.dh);
        pClDevice.reset();
        device.release();
    }
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenDeviceImplIsCreatedWithOsCsrThenDebuggerIsNotifiedWithCorrectDeviceHandle) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.debuggerSupported) {
        DebuggerLibraryInterceptor interceptor;
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);
        DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useHwCsr = true;

        HardwareInfo *hwInfo = nullptr;
        ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);

        hwInfo->capabilityTable.instrumentationEnabled = true;
        unique_ptr<MockDevice> device(Device::create<MockDevice>(executionEnvironment, 0));
        unique_ptr<MockClDevice> pClDevice(new MockClDevice{device.get()});

        ASSERT_NE(nullptr, device->getGpgpuCommandStreamReceiver().getOSInterface());

        EXPECT_TRUE(interceptor.newDeviceCalled);
        uint32_t deviceHandleExpected = device->getGpgpuCommandStreamReceiver().getOSInterface()->getDeviceHandle();
        EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandleExpected)), interceptor.newDeviceArgIn.dh);
        device.release();
    }
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenDeviceIsCreatedThenDebuggerIsNotCreatedInitializedAndNotNotified) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(false);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    EXPECT_EQ(nullptr, device->getDebugger());
    EXPECT_FALSE(interceptor.initCalled);
    EXPECT_FALSE(interceptor.newDeviceCalled);
}

TEST(SourceLevelDebugger, givenTwoRootDevicesWhenSecondIsCreatedThenNotCreatingNewSourceLevelDebugger) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.debuggerSupported) {
        DebuggerLibraryInterceptor interceptor;
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);
        DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

        ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(2);

        std::unique_ptr<Device> device1(Device::create<MockDevice>(executionEnvironment, 0u));
        EXPECT_NE(nullptr, executionEnvironment->memoryManager);
        EXPECT_TRUE(interceptor.initCalled);

        interceptor.initCalled = false;
        std::unique_ptr<Device> device2(Device::create<MockDevice>(executionEnvironment, 1u));
        EXPECT_NE(nullptr, executionEnvironment->memoryManager);
        EXPECT_FALSE(interceptor.initCalled);
    }
}

TEST(SourceLevelDebugger, givenMultipleRootDevicesWhenTheyAreCreatedTheyAllReuseTheSameSourceLevelDebugger) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.debuggerSupported) {
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);

        ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(2);
        std::unique_ptr<Device> device1(Device::create<NEO::MockDevice>(executionEnvironment, 0u));
        auto sourceLevelDebugger = device1->getDebugger();
        std::unique_ptr<Device> device2(Device::create<NEO::MockDevice>(executionEnvironment, 1u));
        EXPECT_EQ(sourceLevelDebugger, device2->getDebugger());
    }
}
