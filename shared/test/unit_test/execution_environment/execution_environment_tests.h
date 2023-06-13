/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

class ExecutionEnvironmentSortTests : public ::testing::Test {
  public:
    void SetUp() override;

    void setupOsSpecifcEnvironment(uint32_t rootDeviceIndex);

    DebugManagerStateRestore restorer;
    ExecutionEnvironment executionEnvironment{};
    static const auto numRootDevices = 6;
    NEO::PhysicalDevicePciBusInfo inputBusInfos[numRootDevices] = {{3, 1, 2, 1}, {0, 0, 2, 0}, {0, 1, 3, 0}, {0, 1, 2, 1}, {0, 0, 2, 1}, {3, 1, 2, 0}};
};
} // namespace NEO
