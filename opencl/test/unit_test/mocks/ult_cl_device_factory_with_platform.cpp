/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "shared/test/common/mocks/ult_device_factory.h"

#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

UltClDeviceFactoryWithPlatform::UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount) {
    initialize(rootDevicesCount, subDevicesCount, new ClExecutionEnvironment(), nullptr);
}

UltClDeviceFactoryWithPlatform::UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment) {
    initialize(rootDevicesCount, subDevicesCount, clExecutionEnvironment, nullptr);
}

UltClDeviceFactoryWithPlatform::UltClDeviceFactoryWithPlatform(uint32_t rootDevicesCount, uint32_t subDevicesCount, MemoryManager *memoryManager) {
    initialize(rootDevicesCount, subDevicesCount, new ClExecutionEnvironment(), memoryManager);
}

void UltClDeviceFactoryWithPlatform::initialize(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment, MemoryManager *memoryManager) {
    if (!NEO::platform(clExecutionEnvironment)) {
        NEO::constructPlatform(clExecutionEnvironment);
        cleanupPlatformOnDestruction = true;
    }

    UltClDeviceFactory::initialize(rootDevicesCount, subDevicesCount, clExecutionEnvironment, memoryManager);
    NEO::initPlatform(rootDevices);
}

UltClDeviceFactoryWithPlatform::~UltClDeviceFactoryWithPlatform() {
    if (cleanupPlatformOnDestruction) {
        NEO::cleanupPlatform(rootDevices[0]->getExecutionEnvironment());
    }
}
