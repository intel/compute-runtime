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

template <typename T = SourceLevelDebugger>
class MockDeviceWithActiveDebugger : public MockDevice {
  public:
    class MockOsLibrary : public OsLibrary {
      public:
        void *getProcAddress(const std::string &procName) override {
            return nullptr;
        }
        bool isLoaded() override {
            return false;
        }
    };
    MockDeviceWithActiveDebugger(const HardwareInfo &hwInfo, bool isRootDevice = true) : MockDevice(hwInfo, isRootDevice) {
        sourceLevelDebuggerCreated = new T(new MockOsLibrary);
        sourceLevelDebugger.reset(sourceLevelDebuggerCreated);
    }

    void initializeCaps() override {
        MockDevice::initializeCaps();
        this->setSourceLevelDebuggerActive(true);
    }

    T *getSourceLevelDebugger() {
        return sourceLevelDebuggerCreated;
    }
    T *sourceLevelDebuggerCreated = nullptr;
};

TEST(DeviceCreation, givenDeviceWithMidThreadPreemptionAndDebuggingActiveWhenDeviceIsCreatedThenCorrectSipKernelIsCreated) {

    DebugManagerStateRestore dbgRestorer;
    {
        BuiltIns::shutDown();

        std::unique_ptr<MockBuiltins> mockBuiltins(new MockBuiltins);
        EXPECT_EQ(nullptr, mockBuiltins->peekCurrentInstance());
        mockBuiltins->overrideGlobalBuiltins();
        EXPECT_EQ(mockBuiltins.get(), mockBuiltins->peekCurrentInstance());
        EXPECT_FALSE(mockBuiltins->getSipKernelCalled);

        DebugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::MidThread);
        auto device = std::unique_ptr<MockDeviceWithActiveDebugger<>>(Device::create<MockDeviceWithActiveDebugger<>>(nullptr));

        EXPECT_TRUE(mockBuiltins->getSipKernelCalled);
        EXPECT_LE(SipKernelType::DbgCsr, mockBuiltins->getSipKernelType);
        mockBuiltins->restoreGlobalBuiltins();
        //make sure to release builtins prior to device as they use device
        mockBuiltins.reset();
    }
}

TEST(DeviceCreation, givenDeviceWithDisabledPreemptionAndDebuggingActiveWhenDeviceIsCreatedThenCorrectSipKernelIsCreated) {

    DebugManagerStateRestore dbgRestorer;
    {
        BuiltIns::shutDown();

        std::unique_ptr<MockBuiltins> mockBuiltins(new MockBuiltins);
        EXPECT_EQ(nullptr, mockBuiltins->peekCurrentInstance());
        mockBuiltins->overrideGlobalBuiltins();
        EXPECT_EQ(mockBuiltins.get(), mockBuiltins->peekCurrentInstance());
        EXPECT_FALSE(mockBuiltins->getSipKernelCalled);

        DebugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::Disabled);
        auto device = std::unique_ptr<MockDeviceWithActiveDebugger<>>(Device::create<MockDeviceWithActiveDebugger<>>(nullptr));

        EXPECT_TRUE(mockBuiltins->getSipKernelCalled);
        EXPECT_LE(SipKernelType::DbgCsr, mockBuiltins->getSipKernelType);
        mockBuiltins->restoreGlobalBuiltins();
        //make sure to release builtins prior to device as they use device
        mockBuiltins.reset();
    }
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsDestructedThenSourceLevelDebuggerIsNotified) {
    auto device = std::unique_ptr<MockDeviceWithActiveDebugger<GMockSourceLevelDebugger>>(Device::create<MockDeviceWithActiveDebugger<GMockSourceLevelDebugger>>(nullptr));
    GMockSourceLevelDebugger *gmock = device->getSourceLevelDebugger();
    EXPECT_CALL(*gmock, notifyDeviceDestruction()).Times(1);
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsCreatedThenPreemptionIsDisabled) {
    auto device = std::unique_ptr<MockDeviceWithActiveDebugger<MockActiveSourceLevelDebugger>>(Device::create<MockDeviceWithActiveDebugger<MockActiveSourceLevelDebugger>>(nullptr));
    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
}
