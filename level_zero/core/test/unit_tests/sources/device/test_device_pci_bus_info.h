/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

namespace NEO {
struct PhysicalDevicePciBusInfo;
struct UltDeviceFactory;
class ExecutionEnvironment;
} // namespace NEO

namespace L0 {
namespace ult {

struct PciBusInfoTest : public ::testing::Test {
    void setPciBusInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhysicalDevicePciBusInfo &pciBusInfo, const uint32_t rootDeviceIndex);
    DebugManagerStateRestore restorer;
};

struct PciBusOrderingTest : public PciBusInfoTest, public ::testing::WithParamInterface<bool> {
};

} // namespace ult
} // namespace L0
