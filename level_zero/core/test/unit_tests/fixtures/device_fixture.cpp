/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

void DeviceFixture::SetUp() { // NOLINT(readability-identifier-naming)
    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];
}

void DeviceFixture::TearDown() { // NOLINT(readability-identifier-naming)
}

void MultiDeviceFixture::SetUp() { // NOLINT(readability-identifier-naming)
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    auto executionEnvironment = new NEO::ExecutionEnvironment;
    auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    ze_result_t res = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

void MultiDeviceFixture::TearDown() { // NOLINT(readability-identifier-naming)
}

void ContextFixture::SetUp() {
    DeviceFixture::SetUp();

    ze_context_handle_t hContext = {};
    ze_context_desc_t desc;
    ze_result_t res = driverHandle->createContext(&desc, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, hContext);
    context = L0::Context::fromHandle(hContext);
}

void ContextFixture::TearDown() {
    context->destroy();
    DeviceFixture::TearDown();
}

} // namespace ult
} // namespace L0
