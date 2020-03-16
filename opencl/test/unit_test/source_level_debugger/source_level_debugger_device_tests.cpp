/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/mocks/mock_os_library.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_builtins.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_source_level_debugger.h"
#include "test.h"

using PreambleTest = ::testing::Test;
using namespace NEO;

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

    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(gmock);
    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));

    EXPECT_CALL(*gmock, notifyDeviceDestruction()).Times(1);
}

TEST(DeviceWithSourceLevelDebugger, givenDeviceWithSourceLevelDebuggerActiveWhenDeviceIsCreatedThenPreemptionIsDisabled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));
    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));

    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
}
