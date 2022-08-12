/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

using namespace NEO;

class MemoryAllocatorMultiDeviceSystemSpecificFixture {
  public:
    void setUp(ExecutionEnvironment &executionEnvironment);
    void tearDown(ExecutionEnvironment &executionEnvironment);

    std::unique_ptr<Gmm> gmm;
};

template <uint32_t numRootDevices>
class MemoryAllocatorMultiDeviceFixture : public MemoryManagementFixture, public MemoryAllocatorMultiDeviceSystemSpecificFixture, public ::testing::TestWithParam<bool> {
  public:
    void SetUp() override {
        MemoryManagementFixture::setUp();

        isOsAgnosticMemoryManager = GetParam();
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        ultHwConfig.forceOsAgnosticMemoryManager = isOsAgnosticMemoryManager;

        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), true, numRootDevices);
        devices = DeviceFactory::createDevices(*executionEnvironment);
        memoryManager = executionEnvironment->memoryManager.get();

        if (!isOsAgnosticMemoryManager) {
            MemoryAllocatorMultiDeviceSystemSpecificFixture::setUp(*executionEnvironment);
        }
    }

    void TearDown() override {
        if (!isOsAgnosticMemoryManager) {
            MemoryAllocatorMultiDeviceSystemSpecificFixture::tearDown(*executionEnvironment);
        }
    }

    uint32_t getNumRootDevices() { return numRootDevices; }

  protected:
    std::vector<std::unique_ptr<Device>> devices;
    ExecutionEnvironment *executionEnvironment = nullptr;
    MemoryManager *memoryManager = nullptr;
    DebugManagerStateRestore restorer;
    bool isOsAgnosticMemoryManager;
};

template <uint32_t numRootDevices, uint32_t numSubDevices>
class MemoryAllocatorMultiDeviceAndMultiTileFixture : public MemoryManagementFixture, public MemoryAllocatorMultiDeviceSystemSpecificFixture, public ::testing::TestWithParam<bool> {
  public:
    void SetUp() override {
        MemoryManagementFixture::setUp();

        isOsAgnosticMemoryManager = GetParam();
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        DebugManager.flags.CreateMultipleRootDevices.set(numSubDevices);
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        ultHwConfig.forceOsAgnosticMemoryManager = isOsAgnosticMemoryManager;

        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), true, numRootDevices);
        devices = DeviceFactory::createDevices(*executionEnvironment);
        memoryManager = executionEnvironment->memoryManager.get();

        if (!isOsAgnosticMemoryManager) {
            MemoryAllocatorMultiDeviceSystemSpecificFixture::setUp(*executionEnvironment);
        }
    }

    void TearDown() override {
        if (!isOsAgnosticMemoryManager) {
            MemoryAllocatorMultiDeviceSystemSpecificFixture::tearDown(*executionEnvironment);
        }
    }

    uint32_t getNumRootDevices() { return numRootDevices; }

  protected:
    std::vector<std::unique_ptr<Device>> devices;
    ExecutionEnvironment *executionEnvironment = nullptr;
    MemoryManager *memoryManager = nullptr;
    DebugManagerStateRestore restorer;
    bool isOsAgnosticMemoryManager;
};
