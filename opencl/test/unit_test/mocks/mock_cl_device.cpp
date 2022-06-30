/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

bool &MockClDevice::createSingleDevice = MockDevice::createSingleDevice;

decltype(&createCommandStream) &MockClDevice::createCommandStreamReceiverFunc = MockDevice::createCommandStreamReceiverFunc;

MockClDevice::MockClDevice(MockDevice *pMockDevice)
    : ClDevice(*pMockDevice, platform()), device(*pMockDevice), sharedDeviceInfo(device.deviceInfo),
      executionEnvironment(pMockDevice->executionEnvironment), allEngines(pMockDevice->allEngines) {
}

bool MockClDevice::areOcl21FeaturesSupported() const {
    return device.getHardwareInfo().capabilityTable.supportsOcl21Features;
}
