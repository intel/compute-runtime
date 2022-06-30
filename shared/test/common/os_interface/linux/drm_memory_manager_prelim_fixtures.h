/*
 * Copyright (C) 2022 Intel Corporation
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

class DrmMemoryManagerLocalMemoryPrelimTest : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.EnableLocalMemory.set(localMemoryEnabled);

        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());

        mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        auto memoryInfo = new MockExtendedMemoryInfo();
        mock->memoryInfo.reset(memoryInfo);

        auto &multiTileArchInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        multiTileArchInfo.TileCount = (memoryInfo->getDrmRegionInfos().size() - 1);
        multiTileArchInfo.IsValid = (multiTileArchInfo.TileCount > 0);

        mock->queryEngineInfo();

        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

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

class DrmMemoryManagerLocalMemoryWithCustomPrelimMockTest : public ::testing::Test {
  public:
    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

        mock = new DrmMockCustomPrelim(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
        memoryManager = std::make_unique<TestedDrmMemoryManager>(localMemoryEnabled, false, false, *executionEnvironment);
    }

  protected:
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMockCustomPrelim *mock;
    ExecutionEnvironment *executionEnvironment;
};

class DrmMemoryManagerFixturePrelim : public DrmMemoryManagerFixture {
  public:
    void SetUp() override {
        regionInfo.resize(2);
        regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 1};
        regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

        MemoryManagementFixture::SetUp();
        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        mock = new DrmMockCustomPrelim(*executionEnvironment->rootDeviceEnvironments[0]);
        mock->memoryInfo.reset(new MemoryInfo(regionInfo));

        DrmMemoryManagerFixture::SetUp(mock, true);
    }

    void TearDown() override {
        mock->testIoctls();
        DrmMemoryManagerFixture::TearDown();
    }

    std::vector<MemoryRegion> regionInfo;
    DrmMockCustomPrelim *mock;
};
