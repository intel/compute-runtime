/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/mocks/windows/mock_wddm_eudebug.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include <algorithm>
#include <memory>

using namespace NEO;

namespace L0 {
namespace ult {

struct L0DebuggerWindowsFixture {
    void SetUp() {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->setDebuggingEnabled();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        auto osEnvironment = new OsEnvironmentWin();
        gdi = new MockGdi();
        osEnvironment->gdi.reset(gdi);
        executionEnvironment->osEnvironment.reset(osEnvironment);
        wddm = new WddmEuDebugInterfaceMock(*rootDeviceEnvironment);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        wddm->init();

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
    WddmEuDebugInterfaceMock *wddm = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    MockGdi *gdi = nullptr;
};

using L0DebuggerWindowsTest = Test<L0DebuggerWindowsFixture>;

TEST_F(L0DebuggerWindowsTest, givenProgramDebuggingEnabledWhenDriverHandleIsCreatedThenItAllocatesL0Debugger) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());
    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

TEST_F(L0DebuggerWindowsTest, givenWindowsOSWhenL0DebuggerIsCreatedAddressModeIsSingleSpace) {
    EXPECT_TRUE(device->getL0Debugger()->getSingleAddressSpaceSbaTracking());
}

} // namespace ult
} // namespace L0
