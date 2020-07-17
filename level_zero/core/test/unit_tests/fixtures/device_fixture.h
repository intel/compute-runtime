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

#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {

struct DeviceFixture {
    virtual void SetUp() { // NOLINT(readability-identifier-naming)
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
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
        auto executionEnvironment = new NEO::ExecutionEnvironment;
        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
    }

    virtual void TearDown() { // NOLINT(readability-identifier-naming)
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::vector<L0::Device *> devices;
    const uint32_t numRootDevices = 4u;
};

} // namespace ult
} // namespace L0
