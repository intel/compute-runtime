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
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"

#include "test.h"

using PreambleTest = ::testing::Test;
using namespace OCLRT;

class MockOsLibrary : public OsLibrary {
  public:
    void *getProcAddress(const std::string &procName) override {
        return nullptr;
    }
    bool isLoaded() override {
        return false;
    }
};

class MockDeviceWithDebuggerActive : public MockDevice {
  public:
    MockDeviceWithDebuggerActive(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment) : MockDevice(hwInfo, executionEnvironment) {}
    void initializeCaps() override {
        MockDevice::initializeCaps();
        this->setSourceLevelDebuggerActive(true);
    }
};

TEST(DeviceCreation, givenDeviceWithMidThreadPreemptionAndDebuggingActiveWhenDeviceIsCreatedThenCorrectSipKernelIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    {
        auto builtIns = new MockBuiltins();
        ASSERT_FALSE(builtIns->getSipKernelCalled);

        DebugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::MidThread);
        auto exeEnv = new ExecutionEnvironment;
        exeEnv->sourceLevelDebugger.reset(new SourceLevelDebugger(new MockOsLibrary));
        exeEnv->builtins.reset(builtIns);
        auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv));

        ASSERT_EQ(builtIns, &device->getBuiltIns());
        EXPECT_TRUE(builtIns->getSipKernelCalled);
        EXPECT_LE(SipKernelType::DbgCsr, builtIns->getSipKernelType);
    }
}

TEST(DeviceCreation, givenDeviceWithDisabledPreemptionAndDebuggingActiveWhenDeviceIsCreatedThenCorrectSipKernelIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    {
        auto builtIns = new MockBuiltins();
        ASSERT_FALSE(builtIns->getSipKernelCalled);

        DebugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::Disabled);

        auto exeEnv = new ExecutionEnvironment;
        exeEnv->sourceLevelDebugger.reset(new SourceLevelDebugger(new MockOsLibrary));
        exeEnv->builtins.reset(builtIns);
        auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv));

        ASSERT_EQ(builtIns, &device->getBuiltIns());
        EXPECT_TRUE(builtIns->getSipKernelCalled);
        EXPECT_LE(SipKernelType::DbgCsr, builtIns->getSipKernelType);
    }
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsDestructedThenSourceLevelDebuggerIsNotified) {
    auto exeEnv = new ExecutionEnvironment;
    auto gmock = new ::testing::NiceMock<GMockSourceLevelDebugger>(new MockOsLibrary);
    exeEnv->sourceLevelDebugger.reset(gmock);
    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv));

    EXPECT_CALL(*gmock, notifyDeviceDestruction()).Times(1);
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsCreatedThenPreemptionIsDisabled) {
    auto exeEnv = new ExecutionEnvironment;
    exeEnv->sourceLevelDebugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));
    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv));

    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
}
