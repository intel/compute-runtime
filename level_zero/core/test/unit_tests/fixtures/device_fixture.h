/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/mocks/mock_device.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {

struct DeviceFixture {
    virtual void SetUp() { // NOLINT(readability-identifier-naming)
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    virtual void TearDown() { // NOLINT(readability-identifier-naming)
    }
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
};

struct MultiDeviceFixture {
    virtual void SetUp() { // NOLINT(readability-identifier-naming)
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        auto executionEnvironment = new NEO::ExecutionEnvironment;
        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }

    virtual void TearDown() { // NOLINT(readability-identifier-naming)
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::vector<NEO::Device *> devices;
    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

struct ContextFixture : DeviceFixture {
    void SetUp() override {
        DeviceFixture::SetUp();

        ze_context_handle_t hContext = {};
        ze_context_desc_t desc;
        ze_result_t res = driverHandle->createContext(&desc, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_NE(nullptr, hContext);
        context = L0::Context::fromHandle(hContext);
    }

    void TearDown() override {
        context->destroy();
        DeviceFixture::TearDown();
    }
    L0::Context *context = nullptr;
};

} // namespace ult
} // namespace L0
