/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/cl_execution_environment_helper.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {

ExecutionEnvironment *getClExecutionEnvironmentImpl(HardwareInfo *&hwInfo, uint32_t rootDeviceEnvironments) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(rootDeviceEnvironments);
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(rootDeviceEnvironments);
    hwInfo = nullptr;
    DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    executionEnvironment->initializeMemoryManager();

    return executionEnvironment;
}
} // namespace NEO
