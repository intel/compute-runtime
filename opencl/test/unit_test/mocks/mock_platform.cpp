/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "shared/source/device/device.h"
#include "shared/source/os_interface/device_factory.h"

namespace NEO {

bool initPlatform() {
    auto pPlatform = platform();
    return pPlatform->initialize(DeviceFactory::createDevices(*pPlatform->peekExecutionEnvironment()));
}
bool MockPlatform::initializeWithNewDevices() {
    return Platform::initialize(DeviceFactory::createDevices(executionEnvironment));
}
} // namespace NEO
