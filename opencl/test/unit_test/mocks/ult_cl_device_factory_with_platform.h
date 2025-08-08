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

class UltClDeviceFactoryWithPlatform : public UltClDeviceFactory {
  public:
    UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount);
    UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount, MemoryManager *memoryManager);
    UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment);
    ~UltClDeviceFactoryWithPlatform();

  protected:
    bool cleanupPlatformOnDestruction = false;
    void initialize(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment, MemoryManager *memoryManager);
};

} // namespace NEO
