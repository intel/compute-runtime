/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/mocks/linux/mock_drm_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_builtins.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture.h"

#include <memory>

namespace NEO {

extern std::vector<void *> mmapVector;

class DrmMemoryManagerBasic : public ::testing::Test {
  public:
    DrmMemoryManagerBasic() : executionEnvironment(defaultHwInfo.get(), false, numRootDevices){};
    void SetUp() override {
        for (auto i = 0u; i < numRootDevices; i++) {
            executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
            auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[i]);
            executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
            executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, i);
        }
    }
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
    MockExecutionEnvironment executionEnvironment;
};

class DrmMemoryManagerFixture : public MemoryManagementFixture {
  public:
    DrmMockCustom *mock = nullptr;
    bool dontTestIoctlInTearDown = false;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
    TestedDrmMemoryManager *memoryManager = nullptr;
    MockClDevice *device = nullptr;

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        SetUp(new DrmMockCustom, false);
    }

    void SetUp(DrmMockCustom *mock, bool localMemoryEnabled) {
        DebugManager.flags.DeferOsContextInitialization.set(0);

        environmentWrapper.setCsrType<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>>();
        allocationData.rootDeviceIndex = rootDeviceIndex;
        this->mock = mock;
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
        executionEnvironment->incRefInternal();
        for (auto i = 0u; i < numRootDevices; i++) {
            auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[i].get();
            rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom()));
            rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i);
            rootDeviceEnvironment->builtins.reset(new MockBuiltins);
        }

        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));

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

        auto csr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
        if (csr->globalFenceAllocation) {
            mock->ioctl_expected.gemClose += static_cast<int>(device->engines.size());
            mock->ioctl_expected.gemWait += static_cast<int>(device->engines.size());
        }
        if (csr->getPreemptionAllocation()) {
            mock->ioctl_expected.gemClose += static_cast<int>(device->engines.size());
            mock->ioctl_expected.gemWait += static_cast<int>(device->engines.size());
        }
        mock->ioctl_expected.gemWait += additionalDestroyDeviceIoctls.gemWait.load();
        mock->ioctl_expected.gemClose += additionalDestroyDeviceIoctls.gemClose.load();
        delete device;
        if (dontTestIoctlInTearDown) {
            mock->reset();
        }
        mock->testIoctls();
        executionEnvironment->decRefInternal();
        MemoryManagementFixture::TearDown();
        mmapVector.clear();
    }

  protected:
    ExecutionEnvironment *executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    AllocationData allocationData{};
    DrmMockCustom::Ioctls additionalDestroyDeviceIoctls{};
    EnvironmentWithCsrWrapper environmentWrapper;
    DebugManagerStateRestore restore;
};

class DrmMemoryManagerWithLocalMemoryFixture : public DrmMemoryManagerFixture {
  public:
    void SetUp() override {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = false;

        MemoryManagementFixture::SetUp();
        DrmMemoryManagerFixture::SetUp(new DrmMockCustom, true);
    }
    void TearDown() override {
        DrmMemoryManagerFixture::TearDown();
    }
    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMockCustom *mock;

    void SetUp() {
        DebugManager.flags.DeferOsContextInitialization.set(0);

        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        uint32_t i = 0;
        for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
            rootDeviceEnvironment->setHwInfo(defaultHwInfo.get());
            rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom));
            rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i);
            i++;
        }
        mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
        constructPlatform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);
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
    DebugManagerStateRestore restore;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};
} // namespace NEO
