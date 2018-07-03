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

#include "runtime/device/device.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/program/kernel_info.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/libult/source_level_debugger_library.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace OCLRT;
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

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    unique_ptr<MockDevice> device(new MockDevice(*platformDevices[0]));
    EXPECT_TRUE(interceptor.initCalled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenDeviceImplIsCreatedThenDebuggerIsNotified) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.sourceLevelDebuggerSupported) {
        DebuggerLibraryInterceptor interceptor;
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);
        DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

        unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        EXPECT_TRUE(interceptor.newDeviceCalled);
        uint32_t deviceHandleExpected = device->getCommandStreamReceiver().getOSInterface() != nullptr ? device->getCommandStreamReceiver().getOSInterface()->getDeviceHandle() : 0;
        EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandleExpected)), interceptor.newDeviceArgIn.dh);
    }
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenDeviceImplIsCreatedWithOsCsrThenDebuggerIsNotifiedWithCorrectDeviceHandle) {
    DebuggerLibraryRestorer restorer;

    if (platformDevices[0]->capabilityTable.sourceLevelDebuggerSupported) {
        DebuggerLibraryInterceptor interceptor;
        DebuggerLibrary::setLibraryAvailable(true);
        DebuggerLibrary::setDebuggerActive(true);
        DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

        overrideCommandStreamReceiverCreation = true;

        // Device::create must be used to create correct OS memory manager
        unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<Device>(platformDevices[0]));
        ASSERT_NE(nullptr, device->getCommandStreamReceiver().getOSInterface());

        EXPECT_TRUE(interceptor.newDeviceCalled);
        uint32_t deviceHandleExpected = device->getCommandStreamReceiver().getOSInterface()->getDeviceHandle();
        EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(deviceHandleExpected)), interceptor.newDeviceArgIn.dh);
    }
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenDeviceIsCreatedThenDebuggerIsNotCreatedInitializedAndNotNotified) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::setDebuggerActive(false);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    unique_ptr<MockDevice> device(DeviceHelper<>::create());

    EXPECT_EQ(nullptr, device->sourceLevelDebugger.get());
    EXPECT_FALSE(interceptor.initCalled);
    EXPECT_FALSE(interceptor.newDeviceCalled);
}
