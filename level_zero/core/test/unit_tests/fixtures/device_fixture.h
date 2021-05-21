/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
struct Context;
struct Device;

namespace ult {

struct DeviceFixture {
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    virtual void SetUp();    // NOLINT(readability-identifier-naming)
    virtual void TearDown(); // NOLINT(readability-identifier-naming)

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::ContextImp *context = nullptr;
};

struct MultiDeviceFixture {
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    virtual void SetUp();    // NOLINT(readability-identifier-naming)
    virtual void TearDown(); // NOLINT(readability-identifier-naming)

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::vector<NEO::Device *> devices;
    uint32_t numRootDevices = 2u;
    uint32_t numSubDevices = 2u;
    L0::ContextImp *context = nullptr;
};

struct ContextFixture : DeviceFixture {
    void SetUp() override;
    void TearDown() override;
};

} // namespace ult
} // namespace L0
