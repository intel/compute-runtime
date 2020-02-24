/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

template <uint32_t numRootDevices>
class MemoryAllocatorMultiDeviceFixture : public MemoryManagementFixture, public ::testing::TestWithParam<bool> {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();

        isOsAgnosticMemoryManager = GetParam();
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useMockedGetDevicesFunc = false;
        ultHwConfig.forceOsAgnosticMemoryManager = isOsAgnosticMemoryManager;

        initPlatform();
        executionEnvironment = platform()->peekExecutionEnvironment();
        memoryManager = executionEnvironment->memoryManager.get();
    }

    uint32_t getNumRootDevices() { return numRootDevices; }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    MemoryManager *memoryManager = nullptr;
    DebugManagerStateRestore restorer;
    bool isOsAgnosticMemoryManager;
};
