/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <vector>

namespace NEO {
class ExecutionEnvironment;
class MockDevice;
class SubDevice;

struct UltDeviceFactory {
    UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount);
    UltDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount, ExecutionEnvironment &executionEnvironment);
    ~UltDeviceFactory();

    static void prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment, uint32_t rootDevicesCount);
    static bool initializeMemoryManager(ExecutionEnvironment &executionEnvironment);

    std::vector<MockDevice *> rootDevices;
    std::vector<SubDevice *> subDevices;
};

} // namespace NEO
