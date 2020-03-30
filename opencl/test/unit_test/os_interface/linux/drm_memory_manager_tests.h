/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/mocks/linux/mock_drm_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/linux/mock_drm_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture.h"

#include <memory>

namespace NEO {

using AllocationData = TestedDrmMemoryManager::AllocationData;

class DrmMemoryManagerBasic : public ::testing::Test {
  public:
    DrmMemoryManagerBasic() : executionEnvironment(defaultHwInfo.get(), false, numRootDevices){};
    void SetUp() override {
        for (auto i = 0u; i < numRootDevices; i++) {
            executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
            executionEnvironment.rootDeviceEnvironments[i]->osInterface->get()->setDrm(Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[i]));
            executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();
        }
    }
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
    MockExecutionEnvironment executionEnvironment;
};

class DrmMemoryManagerFixture : public MemoryManagementFixture {
  public:
    DrmMockCustom *mock = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
    TestedDrmMemoryManager *memoryManager = nullptr;
    MockClDevice *device = nullptr;

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        SetUp(new DrmMockCustom, false);
    }

    void SetUp(DrmMockCustom *mock, bool localMemoryEnabled) {
        environmentWrapper.setCsrType<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>>();
        allocationData.rootDeviceIndex = rootDeviceIndex;
        this->mock = mock;
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
        executionEnvironment->incRefInternal();
        for (auto i = 0u; i < numRootDevices; i++) {
            auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[i].get();
            rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment->osInterface->get()->setDrm(new DrmMockCustom());
        }

        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get();
        rootDeviceEnvironment->osInterface->get()->setDrm(mock);

        memoryManager = new (std::nothrow) TestedDrmMemoryManager(localMemoryEnabled, false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
        device = new MockClDevice{MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex)};
        mock->reset();
    }

    void TearDown() override {
        mock->testIoctls();
        mock->reset();
        mock->ioctl_expected.contextDestroy = static_cast<int>(device->engines.size());
        mock->ioctl_expected.gemClose = static_cast<int>(device->engines.size());
        mock->ioctl_expected.gemWait = static_cast<int>(device->engines.size());

        if (device->getDefaultEngine().commandStreamReceiver->getPreemptionAllocation()) {
            mock->ioctl_expected.gemClose += static_cast<int>(device->engines.size());
            mock->ioctl_expected.gemWait += static_cast<int>(device->engines.size());
        }
        mock->ioctl_expected.gemWait += additionalDestroyDeviceIoctls.gemWait.load();
        mock->ioctl_expected.gemClose += additionalDestroyDeviceIoctls.gemClose.load();
        delete device;
        mock->testIoctls();
        executionEnvironment->decRefInternal();
        MemoryManagementFixture::TearDown();
    }

  protected:
    ExecutionEnvironment *executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    AllocationData allocationData{};
    DrmMockCustom::Ioctls additionalDestroyDeviceIoctls{};
    EnvironmentWithCsrWrapper environmentWrapper;
};

class DrmMemoryManagerWithLocalMemoryFixture : public DrmMemoryManagerFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        DrmMemoryManagerFixture::SetUp(new DrmMockCustom, true);
    }
    void TearDown() override {
        DrmMemoryManagerFixture::TearDown();
    }
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMockCustom *mock;

    void SetUp() {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
            rootDeviceEnvironment->setHwInfo(defaultHwInfo.get());
            rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment->osInterface->get()->setDrm(new DrmMockCustom);
        }
        mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->getDrm());
        memoryManager.reset(new TestedDrmMemoryManager(*executionEnvironment));

        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
    }

    void TearDown() {
    }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockDevice> device;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};
} // namespace NEO
