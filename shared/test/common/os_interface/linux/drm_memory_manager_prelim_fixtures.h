/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture_prelim.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"

template <bool multipleSubDevices>
class DrmMemoryManagerWithSubDevicesPrelimTest : public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(multipleSubDevices ? 2 : 1);

        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());

        mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        auto memoryInfo = new MockExtendedMemoryInfo(*mock);

        mock->queryEngineInfo();
        mock->memoryInfo.reset(memoryInfo);

        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

        memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
    }

  protected:
    DebugManagerStateRestore restorer{};
    ExecutionEnvironment *executionEnvironment{nullptr};
    DrmQueryMock *mock{nullptr};
    std::unique_ptr<MockDevice> device;
    TestedDrmMemoryManager *memoryManager{nullptr};

    constexpr static uint32_t rootDeviceIndex{0u};
};

class DrmMemoryManagerLocalMemoryPrelimTest : public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(localMemoryEnabled);

        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());

        mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        auto memoryInfo = new MockExtendedMemoryInfo(*mock);
        mock->memoryInfo.reset(memoryInfo);

        auto &multiTileArchInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        multiTileArchInfo.TileCount = (memoryInfo->getDrmRegionInfos().size() - 1);
        multiTileArchInfo.IsValid = (multiTileArchInfo.TileCount > 0);

        mock->queryEngineInfo();

        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

        memoryManager = new TestedDrmMemoryManager(localMemoryEnabled, false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
    }

  protected:
    DebugManagerStateRestore restorer{};
    ExecutionEnvironment *executionEnvironment{nullptr};
    DrmQueryMock *mock{nullptr};
    std::unique_ptr<MockDevice> device;
    TestedDrmMemoryManager *memoryManager{nullptr};

    constexpr static uint32_t rootDeviceIndex{0u};
    constexpr static bool localMemoryEnabled{true};
};
