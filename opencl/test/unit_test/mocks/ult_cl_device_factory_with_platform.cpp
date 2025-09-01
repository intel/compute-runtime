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
    auto platform = NEO::platform(clExecutionEnvironment);
    if (!platform) {
        platform = NEO::constructPlatform(clExecutionEnvironment);
        cleanupPlatformOnDestruction = true;
    }

    pUltDeviceFactory = std::make_unique<UltDeviceFactory>(rootDevicesCount, subDevicesCount, *clExecutionEnvironment);
    if (memoryManager != nullptr) {
        for (auto device : pUltDeviceFactory->rootDevices) {
            device->injectMemoryManager(memoryManager);
        }
    }
    NEO::initPlatform(pUltDeviceFactory->rootDevices);
    auto clDevices = platform->getClDevices();
    for (size_t i = 0; i < platform->getNumDevices(); i++) {
        auto clDevice = static_cast<MockClDevice *>(clDevices[i]);
        for (auto &clSubDevice : clDevice->subDevices) {
            subDevices.push_back(clSubDevice.get());
        }
        rootDevices.push_back(clDevice);
    }
}

UltClDeviceFactoryWithPlatform::~UltClDeviceFactoryWithPlatform() {
    if (cleanupPlatformOnDestruction) {
        NEO::cleanupPlatform(rootDevices[0]->getExecutionEnvironment());
    }
}
