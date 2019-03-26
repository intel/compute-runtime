/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/linux/os_interface.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/linux/mock_drm_memory_manager.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include <memory>

namespace NEO {

using AllocationData = TestedDrmMemoryManager::AllocationData;

class DrmMemoryManagerBasic : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment.osInterface = std::make_unique<OSInterface>();
        executionEnvironment.osInterface->get()->setDrm(Drm::get(0));
    }

    ExecutionEnvironment executionEnvironment;
};

class DrmMemoryManagerFixture : public MemoryManagementFixture {
  public:
    TestedDrmMemoryManager *memoryManager = nullptr;
    DrmMockCustom *mock;
    MockDevice *device = nullptr;

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        this->mock = new DrmMockCustom;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->incRefInternal();
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setDrm(mock);

        memoryManager = new (std::nothrow) TestedDrmMemoryManager(*executionEnvironment);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
        device = MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0);
    }

    void TearDown() override {
        delete device;
        delete memoryManager;
        executionEnvironment->decRefInternal();

        this->mock->testIoctls();

        delete this->mock;
        this->mock = nullptr;
        MemoryManagementFixture::TearDown();
    }

  protected:
    ExecutionEnvironment *executionEnvironment;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    AllocationData allocationData;
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    std::unique_ptr<DrmMockCustom> mock;

    void SetUp() {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->setHwInfo(*platformDevices);
        mock = std::make_unique<DrmMockCustom>();
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setDrm(mock.get());
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
