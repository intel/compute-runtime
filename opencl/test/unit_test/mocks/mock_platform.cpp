/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"

namespace NEO {

bool MockPlatform::initializeWithNewDevices() {
    executionEnvironment.prepareRootDeviceEnvironments(1u);

    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {

        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());

        UnitTestSetter::setRcsExposure(*executionEnvironment.rootDeviceEnvironments[i]);
        UnitTestSetter::setCcsExposure(*executionEnvironment.rootDeviceEnvironments[i]);
    }

    executionEnvironment.calculateMaxOsContextCount();

    return Platform::initialize(DeviceFactory::createDevices(executionEnvironment));
}

bool MockPlatform::initializeAsMockClDevices(std::vector<std::unique_ptr<Device>> devices) {
    bool success = Platform::initialize(std::move(devices));
    if (success) {
        for (size_t i = 0; i < clDevices.size(); i++) {
            auto mockClDevice = new MockClDevice(static_cast<MockDevice *>(&clDevices[i]->getDevice()));
            delete clDevices[i];
            clDevices[i] = mockClDevice;
        }
    }
    return success;
}

Platform *platform() {
    if (platformsImpl->empty()) {
        return nullptr;
    }
    return (*platformsImpl)[0].get();
}

Platform *platform(ExecutionEnvironment *executionEnvironment) {
    for (auto &platform : *platformsImpl) {
        if (platform->peekExecutionEnvironment() == executionEnvironment) {
            return platform.get();
        }
    }
    return nullptr;
}

static std::mutex mutex;

bool initPlatform() {
    std::unique_lock<std::mutex> lock(mutex);
    auto pPlatform = static_cast<MockPlatform *>(platform());
    return pPlatform->initializeAsMockClDevices(DeviceFactory::createDevices(*pPlatform->peekExecutionEnvironment()));
}

bool initPlatform(std::vector<MockDevice *> deviceVector) {
    std::unique_lock<std::mutex> lock(mutex);
    auto device = deviceVector[0];
    auto pPlatform = static_cast<MockPlatform *>(platform(device->getExecutionEnvironment()));

    if (pPlatform->isInitialized()) {
        return true;
    }
    std::vector<std::unique_ptr<Device>> devices;
    for (auto &device : deviceVector) {
        devices.push_back(std::unique_ptr<Device>(device));
    }
    return pPlatform->initializeAsMockClDevices(std::move(devices));
}

Platform *constructPlatform() {
    std::unique_lock<std::mutex> lock(mutex);
    if (platformsImpl->empty()) {
        platformsImpl->push_back(std::make_unique<MockPlatform>(*(new MockClExecutionEnvironment())));
    }
    return (*platformsImpl)[0].get();
}

Platform *constructPlatform(ExecutionEnvironment *executionEnvironment) {
    std::unique_lock<std::mutex> lock(mutex);
    for (auto &platform : *platformsImpl) {
        if (platform->peekExecutionEnvironment() == executionEnvironment) {
            return platform.get();
        }
    }
    platformsImpl->push_back(std::make_unique<MockPlatform>(*executionEnvironment));
    return platformsImpl->back().get();
}

void cleanupPlatform(ExecutionEnvironment *executionEnvironment) {
    std::unique_lock<std::mutex> lock(mutex);
    for (auto it = platformsImpl->begin(); it != platformsImpl->end(); it++) {
        if ((*it)->peekExecutionEnvironment() == executionEnvironment) {
            it = platformsImpl->erase(it);
            break;
        }
    }
}

} // namespace NEO
