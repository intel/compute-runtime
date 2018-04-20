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

#include "runtime/source_level_debugger/source_level_debugger.h"
#include "unit_tests/libult/source_level_debugger_library.h"

#include <gtest/gtest.h>

using namespace OCLRT;

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

class MockSourceLevelDebugger : public SourceLevelDebugger {
  public:
    using SourceLevelDebugger::debuggerLibrary;
    MockSourceLevelDebugger() = default;
    void setActive(bool active) {
        isActive = active;
    }
};

TEST(SourceLevelDebugger, givenNoKernelDebuggerLibraryWhenSourceLevelDebuggerIsCreatedThenLibraryIsNotLoaded) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(false);

    MockSourceLevelDebugger debugger;
    EXPECT_EQ(nullptr, debugger.debuggerLibrary.get());
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryAvailableWhenIsDebuggerActiveIsCalledThenLibraryIsLoadedAndFalseIsReturned) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(true);

    MockSourceLevelDebugger debugger;
    EXPECT_NE(nullptr, debugger.debuggerLibrary.get());
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryAvailableWhenIsDebuggerActiveIsCalledThenTrueIsReturned) {
    DebuggerLibraryRestorer restorer;
    DebuggerLibrary::setLibraryAvailable(true);

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
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    const char source[] = "sourceCode";
    debugger.notifySourceCode(4, source, sizeof(source));

    EXPECT_TRUE(interceptor.sourceCodeCalled);
    EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(4u)), interceptor.sourceCodeArgIn.hDevice);
    EXPECT_EQ(source, interceptor.sourceCodeArgIn.sourceCode);
    EXPECT_EQ(sizeof(source), interceptor.sourceCodeArgIn.sourceCodeSize);
    EXPECT_NE(nullptr, interceptor.sourceCodeArgIn.sourceName);
    EXPECT_NE(0u, interceptor.sourceCodeArgIn.sourceNameMaxLen);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenNotifySourceCodeIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;

    debugger.setActive(false);

    const char source[] = "sourceCode";
    debugger.notifySourceCode(4, source, sizeof(source));
    EXPECT_FALSE(interceptor.sourceCodeCalled);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryActiveWhenNotifyNewDeviceIsCalledThenDebuggerLibraryFunctionIsCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;
    debugger.notifyNewDevice(4);

    EXPECT_TRUE(interceptor.newDeviceCalled);
    EXPECT_EQ(reinterpret_cast<GfxDeviceHandle>(static_cast<uint64_t>(4u)), interceptor.newDeviceArgIn.dh);
}

TEST(SourceLevelDebugger, givenKernelDebuggerLibraryNotActiveWhenNotifyNewDeviceIsCalledThenDebuggerLibraryFunctionIsNotCalled) {
    DebuggerLibraryRestorer restorer;

    DebuggerLibraryInterceptor interceptor;
    DebuggerLibrary::setLibraryAvailable(true);
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
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    char value = '1';
    GfxDbgOption optionArgOut;
    interceptor.optionArgOut = &optionArgOut;
    interceptor.optionArgOut->value = &value;
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
    DebuggerLibrary::injectDebuggerLibraryInterceptor(&interceptor);

    MockSourceLevelDebugger debugger;
    debugger.notifyKernelDebugData();

    EXPECT_TRUE(interceptor.kernelDebugDataCalled);
}