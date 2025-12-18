/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"

namespace NEO {

class MockClDeviceFactory {
  public:
    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) {
        return MockDevice::createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }

    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex = 0) {
        auto executionEnvironment = MockClDevice::prepareExecutionEnvironment(pHwInfo, rootDeviceIndex);
        return MockDevice::createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }
};

} // namespace NEO
