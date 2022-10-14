/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

namespace NEO {
struct PhysicalDevicePciSpeedInfo;
struct UltDeviceFactory;
class ExecutionEnvironment;
} // namespace NEO

namespace L0 {
namespace ult {

struct PciSpeedInfoTest : public ::testing::Test {
    std::unique_ptr<NEO::UltDeviceFactory> createDevices(uint32_t numSubDevices, const NEO::PhysicalDevicePciSpeedInfo &pciSpeedInfo);
    DebugManagerStateRestore restorer;

  private:
    void setPciSpeedInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhysicalDevicePciSpeedInfo &pciSpeedInfo);
};

} // namespace ult
} // namespace L0
