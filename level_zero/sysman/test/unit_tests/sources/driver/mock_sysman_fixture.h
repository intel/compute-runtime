/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/driver/mock_sysman_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

// Standalone OS-agnostic fixture for driver-level sysman tests.
// Uses MockExecutionEnvironment to avoid OS-specific device enumeration side-effects.
// Manages global sysman state (globalSysmanDriverHandle, globalSysmanDriver, driverCount)
// so individual tests start and finish with a clean slate.
class SysmanDriverFixture : public ::testing::Test {
  protected:
    std::unique_ptr<NEO::MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockSysmanDriverHandleImp> driverHandle;

    void SetUp() override {
        executionEnvironment = std::make_unique<NEO::MockExecutionEnvironment>();
        driverHandle = std::make_unique<MockSysmanDriverHandleImp>();
        globalSysmanDriver = driverHandle.get();
        globalSysmanDriverHandle = driverHandle.get();
    }

    void TearDown() override {
        driverHandle.reset();
        globalSysmanDriver = nullptr;
        globalSysmanDriverHandle = nullptr;
        driverCount = 0;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
