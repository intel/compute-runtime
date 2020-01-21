/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/linux/os_interface.h"
#include "runtime/os_interface/linux/drm_memory_operations_handler.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/linux/mock_drm_memory_manager.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include <memory>

namespace NEO {

using AllocationData = TestedDrmMemoryManager::AllocationData;

class DrmMemoryManagerBasic : public ::testing::Test {
  public:
    DrmMemoryManagerBasic() : executionEnvironment(*platformDevices){};
    void SetUp() override {
        executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(Drm::get(0));
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();
    }

    MockExecutionEnvironment executionEnvironment;
};

class DrmMemoryManagerFixture : public MemoryManagementFixture {
  public:
    std::unique_ptr<DrmMockCustom> mock;
    TestedDrmMemoryManager *memoryManager = nullptr;
    MockClDevice *device = nullptr;

    void SetUp() override {
        SetUp(new DrmMockCustom, false);
    }

    void SetUp(DrmMockCustom *mock, bool localMemoryEnabled) {
        MemoryManagementFixture::SetUp();
        this->mock = std::unique_ptr<DrmMockCustom>(mock);
        executionEnvironment = new MockExecutionEnvironment(*platformDevices);
        executionEnvironment->incRefInternal();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setDrm(mock);

        memoryManager = new (std::nothrow) TestedDrmMemoryManager(localMemoryEnabled, false, false, *executionEnvironment);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
        device = new MockClDevice{MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0)};
    }

    void TearDown() override {
        delete device;
        delete memoryManager;
        executionEnvironment->decRefInternal();

        this->mock->testIoctls();

        MemoryManagementFixture::TearDown();
    }

  protected:
    ExecutionEnvironment *executionEnvironment;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    AllocationData allocationData;
};

class DrmMemoryManagerWithLocalMemoryFixture : public DrmMemoryManagerFixture {
  public:
    void SetUp() override {
        DrmMemoryManagerFixture::SetUp(new DrmMockCustom, true);
    }
    void TearDown() override {
        DrmMemoryManagerFixture::TearDown();
    }
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    std::unique_ptr<DrmMockCustom> mock;

    void SetUp() {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->setHwInfo(*platformDevices);
        executionEnvironment->prepareRootDeviceEnvironments(1);
        mock = std::make_unique<DrmMockCustom>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setDrm(mock.get());
        memoryManager.reset(new TestedDrmMemoryManager(*executionEnvironment));

        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0));
    }

    void TearDown() {
    }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockDevice> device;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
};
} // namespace NEO
