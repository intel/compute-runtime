/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

namespace NEO {
class ClExecutionEnvironment;
class MemoryManager;

class UltClDeviceFactoryWithPlatform {
  public:
    UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount);
    UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount, MemoryManager *memoryManager);
    UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment);
    ~UltClDeviceFactoryWithPlatform();
    bool cleanupPlatformOnDestruction = false;

    std::unique_ptr<UltDeviceFactory> pUltDeviceFactory;
    std::vector<MockClDevice *> rootDevices;
    std::vector<ClDevice *> subDevices;

  protected:
    void initialize(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment, MemoryManager *memoryManager);
};

} // namespace NEO
