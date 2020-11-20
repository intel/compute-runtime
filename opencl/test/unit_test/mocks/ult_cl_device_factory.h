/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace NEO {
class ExecutionEnvironment;
class ClDevice;
class MemoryManager;
class MockMemoryManager;
class MockClDevice;
struct UltDeviceFactory;

struct UltClDeviceFactory {
    UltClDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount);
    ~UltClDeviceFactory();

    std::unique_ptr<UltDeviceFactory> pUltDeviceFactory;
    std::vector<MockClDevice *> rootDevices;
    std::vector<ClDevice *> subDevices;
};

} // namespace NEO
