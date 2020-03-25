/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

bool &MockClDevice::createSingleDevice = MockDevice::createSingleDevice;

decltype(&createCommandStream) &MockClDevice::createCommandStreamReceiverFunc = MockDevice::createCommandStreamReceiverFunc;

MockClDevice::MockClDevice(MockDevice *pMockDevice)
    : ClDevice(*pMockDevice, platform()), device(*pMockDevice), sharedDeviceInfo(device.deviceInfo),
      executionEnvironment(pMockDevice->executionEnvironment), mockMemoryManager(pMockDevice->mockMemoryManager),
      engines(pMockDevice->engines) {
}
