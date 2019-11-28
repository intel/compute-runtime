/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace NEO;

template <uint32_t numRootDevices>
class MemoryAllocatorMultiDeviceFixture : public MemoryManagementFixture, public ::testing::TestWithParam<bool> {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();

        isOsAgnosticMemoryManager = GetParam();
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        VariableBackup<bool> overrideDeviceWithDefaultHardwareInfoBackup(&overrideDeviceWithDefaultHardwareInfo, false);
        VariableBackup<bool> overrideMemoryManagerCreationBackup(&overrideMemoryManagerCreation, isOsAgnosticMemoryManager);

        platform()->initialize();
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
