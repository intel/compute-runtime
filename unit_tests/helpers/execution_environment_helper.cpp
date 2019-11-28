/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/execution_environment_helper.h"

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/platform/platform.h"

namespace NEO {

ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo, uint32_t rootDeviceEnvironments) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(rootDeviceEnvironments);
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(rootDeviceEnvironments);
    size_t numDevicesReturned = 0;
    hwInfo = nullptr;
    DeviceFactory::getDevices(numDevicesReturned, *executionEnvironment);
    hwInfo = executionEnvironment->getMutableHardwareInfo();
    executionEnvironment->initializeMemoryManager();

    return executionEnvironment;
}
} // namespace NEO
