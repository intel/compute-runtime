/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

using namespace NEO;

namespace L0 {
namespace ult {

struct L0DebuggerLinuxFixture {
    void SetUp() {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);

        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->initializeMemoryManager();
        auto osInterface = new OSInterface();
        drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setDrm(static_cast<Drm *>(drmMock));

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->enableProgramDebugging = true;

        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() {
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    DrmMockResources *drmMock = nullptr;
};

using L0DebuggerLinuxTest = Test<L0DebuggerLinuxFixture>;

TEST_F(L0DebuggerLinuxTest, givenProgramDebuggingEnabledWhenDriverHandleIsCreatedThenItAllocatesL0Debugger) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());

    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

TEST_F(L0DebuggerLinuxTest, whenDebuggerIsCreatedThenItCallsDrmToRegisterResourceClasses) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());

    EXPECT_TRUE(drmMock->registerClassesCalled);
}

} // namespace ult
} // namespace L0
