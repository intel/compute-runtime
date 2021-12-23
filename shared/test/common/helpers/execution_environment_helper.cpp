/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/execution_environment_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

namespace NEO {

ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo, uint32_t rootDeviceEnvironments) {
    ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment();
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
