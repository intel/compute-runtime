/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/execution_environment_helper.h"

#include "runtime/device/device.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/platform/platform.h"

namespace OCLRT {

ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    size_t numDevicesReturned = 0;
    hwInfo = nullptr;
    DeviceFactory::getDevices(&hwInfo, numDevicesReturned, *executionEnvironment);
    bool enableLocalMemory = HwHelper::get(hwInfo->pPlatform->eRenderCoreFamily).getEnableLocalMemory(*hwInfo);
    executionEnvironment->initializeMemoryManager(Device::getEnabled64kbPages(*hwInfo), enableLocalMemory);

    return executionEnvironment;
}
} // namespace OCLRT
