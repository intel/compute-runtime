/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

namespace NEO {
struct PhyicalDevicePciSpeedInfo;
struct UltDeviceFactory;
class ExecutionEnvironment;
} // namespace NEO

namespace L0 {
namespace ult {

struct PciSpeedInfoTest : public ::testing::Test {
    std::unique_ptr<NEO::UltDeviceFactory> createDevices(uint32_t numSubDevices, const NEO::PhyicalDevicePciSpeedInfo &pciSpeedInfo);
    DebugManagerStateRestore restorer;

  private:
    void setPciSpeedInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhyicalDevicePciSpeedInfo &pciSpeedInfo);
};

} // namespace ult
} // namespace L0
