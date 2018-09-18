/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
    MockDeviceWithDebuggerActive(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : MockDevice(hwInfo, executionEnvironment, deviceIndex) {}
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
        auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv, 0u));

        ASSERT_EQ(builtIns, device->getExecutionEnvironment()->getBuiltIns());
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
        auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv, 0u));

        ASSERT_EQ(builtIns, device->getExecutionEnvironment()->getBuiltIns());
        EXPECT_TRUE(builtIns->getSipKernelCalled);
        EXPECT_LE(SipKernelType::DbgCsr, builtIns->getSipKernelType);
    }
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsDestructedThenSourceLevelDebuggerIsNotified) {
    auto exeEnv = new ExecutionEnvironment;
    auto gmock = new ::testing::NiceMock<GMockSourceLevelDebugger>(new MockOsLibrary);
    exeEnv->sourceLevelDebugger.reset(gmock);
    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv, 0u));

    EXPECT_CALL(*gmock, notifyDeviceDestruction()).Times(1);
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsCreatedThenPreemptionIsDisabled) {
    auto exeEnv = new ExecutionEnvironment;
    exeEnv->sourceLevelDebugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));
    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(nullptr, exeEnv, 0u));

    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
}
