/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/device_factory.h"
#include "unit_tests/helpers/execution_environment_helper.h"

namespace OCLRT {

ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo) {
    ExecutionEnvironment *executionEnvironment = new ExecutionEnvironment;
    size_t numDevicesReturned = 0;
    hwInfo = nullptr;
    DeviceFactory::getDevices(&hwInfo, numDevicesReturned, *executionEnvironment);
    return executionEnvironment;
}
} // namespace OCLRT
