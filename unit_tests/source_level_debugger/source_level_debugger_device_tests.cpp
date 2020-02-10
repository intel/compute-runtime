/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"

using PreambleTest = ::testing::Test;
using namespace NEO;

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
    MockDeviceWithDebuggerActive(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : MockDevice(executionEnvironment, deviceIndex) {}
    void initializeCaps() override {
        MockDevice::initializeCaps();
        this->setDebuggerActive(true);
    }
};

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsDestructedThenSourceLevelDebuggerIsNotified) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto gmock = new ::testing::NiceMock<GMockSourceLevelDebugger>(new MockOsLibrary);
    executionEnvironment->debugger.reset(gmock);
    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));
    std::unique_ptr<MockClDevice> pClDevice(new MockClDevice{device.get()});

    EXPECT_CALL(*gmock, notifyDeviceDestruction()).Times(1);
    device.release();
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsCreatedThenPreemptionIsDisabled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->debugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));
    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));

    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
}
