/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_in_ops_base.h"

#include "level_zero/core/test/unit_tests/mock.h"

#include <cstdint>
#include <memory>

namespace NEO {
class ExecutionEnvironment;
class MockDevice;
struct HardwareInfo;
} // namespace NEO

namespace L0 {
class DriverHandle;
struct Context;
struct Device;

namespace ult {
class MockBuiltins;

struct DeviceFixture {
    DeviceFixture() noexcept;
    virtual ~DeviceFixture();
    void setUp();
    void setUpImpl(NEO::HardwareInfo *hwInfo);
    void tearDown();
    void setupWithExecutionEnvironment(NEO::ExecutionEnvironment &executionEnvironment);

    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::Context *context = nullptr;
    MockBuiltins *mockBuiltIns = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
    NEO::HardwareInfo *hardwareInfo = nullptr;

    const uint32_t rootDeviceIndex = 0u;
    template <typename HelperType>
    HelperType &getHelper() const;

    NEO::BuiltIn::AddressingMode getDefaultBuiltInMode() const;
};

} // namespace ult
} // namespace L0
