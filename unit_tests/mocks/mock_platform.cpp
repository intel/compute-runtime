/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_platform.h"

#include "core/device/device.h"

namespace NEO {

bool initPlatform() {
    auto pPlatform = platform();
    return pPlatform->initialize(DeviceFactory::createDevices(*pPlatform->peekExecutionEnvironment()));
}
bool MockPlatform::initializeWithNewDevices() {
    return Platform::initialize(DeviceFactory::createDevices(executionEnvironment));
}
} // namespace NEO
