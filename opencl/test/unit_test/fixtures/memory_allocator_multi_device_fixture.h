/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

class MemoryAllocatorMultiDeviceSystemSpecificFixture {
  public:
    void SetUp(ExecutionEnvironment &executionEnvironment);
    void TearDown(ExecutionEnvironment &executionEnvironment);

    std::unique_ptr<Gmm> gmm;
};

template <uint32_t numRootDevices>
class MemoryAllocatorMultiDeviceFixture : public MemoryManagementFixture, public MemoryAllocatorMultiDeviceSystemSpecificFixture, public ::testing::TestWithParam<bool> {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();

        isOsAgnosticMemoryManager = GetParam();
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        ultHwConfig.forceOsAgnosticMemoryManager = isOsAgnosticMemoryManager;

        initPlatform();
        executionEnvironment = platform()->peekExecutionEnvironment();
        memoryManager = executionEnvironment->memoryManager.get();

        if (!isOsAgnosticMemoryManager) {
            MemoryAllocatorMultiDeviceSystemSpecificFixture::SetUp(*executionEnvironment);
        }
    }

    void TearDown() override {
        if (!isOsAgnosticMemoryManager) {
            MemoryAllocatorMultiDeviceSystemSpecificFixture::TearDown(*executionEnvironment);
        }
    }

    uint32_t getNumRootDevices() { return numRootDevices; }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    MemoryManager *memoryManager = nullptr;
    DebugManagerStateRestore restorer;
    bool isOsAgnosticMemoryManager;
};
