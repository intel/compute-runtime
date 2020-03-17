/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "shared/source/device/device.h"
#include "shared/source/os_interface/device_factory.h"

#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"

namespace NEO {

bool initPlatform() {
    auto pPlatform = platform();
    return pPlatform->initialize(DeviceFactory::createDevices(*pPlatform->peekExecutionEnvironment()));
}
bool MockPlatform::initializeWithNewDevices() {
    executionEnvironment.prepareRootDeviceEnvironments(1u);
    return Platform::initialize(DeviceFactory::createDevices(executionEnvironment));
}

Platform *platform() {
    if (platformsImpl.empty()) {
        return nullptr;
    }
    return platformsImpl[0].get();
}

Platform *constructPlatform() {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (platformsImpl.empty()) {
        platformsImpl.push_back(std::make_unique<Platform>(*(new MockClExecutionEnvironment())));
    }
    return platformsImpl[0].get();
}
} // namespace NEO
