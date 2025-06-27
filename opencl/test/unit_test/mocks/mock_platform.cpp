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

#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"

namespace NEO {

bool initPlatform() {
    auto pPlatform = platform();
    return pPlatform->initialize(DeviceFactory::createDevices(*pPlatform->peekExecutionEnvironment()));
}
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

Platform *platform() {
    if (platformsImpl->empty()) {
        return nullptr;
    }
    return (*platformsImpl)[0].get();
}

Platform *constructPlatform() {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (platformsImpl->empty()) {
        platformsImpl->push_back(std::make_unique<Platform>(*(new MockClExecutionEnvironment())));
    }
    return (*platformsImpl)[0].get();
}
} // namespace NEO
