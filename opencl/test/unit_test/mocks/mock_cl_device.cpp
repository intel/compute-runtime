/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_ostime.h"
#include "opencl/test/unit_test/tests_configuration.h"

using namespace NEO;

bool &MockClDevice::createSingleDevice = MockDevice::createSingleDevice;

decltype(&createCommandStream) &MockClDevice::createCommandStreamReceiverFunc = MockDevice::createCommandStreamReceiverFunc;

MockClDevice::MockClDevice(MockDevice *pMockDevice)
    : ClDevice(*pMockDevice, platform()), device(*pMockDevice), sharedDeviceInfo(device.deviceInfo),
      executionEnvironment(pMockDevice->executionEnvironment), subdevices(pMockDevice->subdevices),
      mockMemoryManager(pMockDevice->mockMemoryManager), engines(pMockDevice->engines) {
}
