/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/execution_environment_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/platform/platform.h"

namespace NEO {

ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo, uint32_t rootDeviceEnvironments) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(rootDeviceEnvironments);
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(rootDeviceEnvironments);
    size_t numDevicesReturned = 0;
    hwInfo = nullptr;
    DeviceFactory::getDevices(numDevicesReturned, *executionEnvironment);
    hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    executionEnvironment->initializeMemoryManager();

    return executionEnvironment;
}
} // namespace NEO
