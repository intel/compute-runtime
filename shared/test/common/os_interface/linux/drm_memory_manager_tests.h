/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

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
    MockDevice *device = nullptr;

    void SetUp() override {
        MemoryManagementFixture::SetUp();

        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        SetUp(new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]), false);
    }

    void SetUp(DrmMockCustom *mock, bool localMemoryEnabled) {
        ASSERT_NE(nullptr, executionEnvironment);
        executionEnvironment->incRefInternal();
        DebugManager.flags.DeferOsContextInitialization.set(0);

        environmentWrapper.setCsrType<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>>();
        allocationData.rootDeviceIndex = rootDeviceIndex;
        this->mock = mock;
        for (auto i = 0u; i < numRootDevices; i++) {
            auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[i].get();
            rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom(*rootDeviceEnvironment)));
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
        device = MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex);
        mock->reset();
    }

    void TearDown() override {
        mock->testIoctls();
        mock->reset();

        int enginesCount = static_cast<int>(device->getMemoryManager()->getRegisteredEnginesCount());

        mock->ioctl_expected.contextDestroy = enginesCount;
        mock->ioctl_expected.gemClose = enginesCount;
        mock->ioctl_expected.gemWait = enginesCount;

        auto csr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
        if (csr->globalFenceAllocation) {
            mock->ioctl_expected.gemClose += enginesCount;
            mock->ioctl_expected.gemWait += enginesCount;
        }
        if (csr->getPreemptionAllocation()) {
            mock->ioctl_expected.gemClose += enginesCount;
            mock->ioctl_expected.gemWait += enginesCount;
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
    ExecutionEnvironment *executionEnvironment = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    AllocationData allocationData{};
    Ioctls additionalDestroyDeviceIoctls{};
    EnvironmentWithCsrWrapper environmentWrapper;
    DebugManagerStateRestore restore;
};

class DrmMemoryManagerWithLocalMemoryFixture : public DrmMemoryManagerFixture {
  public:
    void SetUp() override {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = false;

        MemoryManagementFixture::SetUp();
        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        DrmMemoryManagerFixture::SetUp(new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]), true);
    }
    void TearDown() override {
        DrmMemoryManagerFixture::TearDown();
    }
    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};

struct MockedMemoryInfo : public NEO::MemoryInfo {
    MockedMemoryInfo(const std::vector<MemoryRegion> &regionInfo) : MemoryInfo(regionInfo) {}
    ~MockedMemoryInfo() override{};

    size_t getMemoryRegionSize(uint32_t memoryBank) override {
        return 1024u;
    }
    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        return 0u;
    }
    uint32_t createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        return 0u;
    }
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMockCustom *mock;

    void SetUp() {
        SetUp(false);
    }

    void SetUp(bool enableLocalMem) {
        DebugManager.flags.DeferOsContextInitialization.set(0);

        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        uint32_t i = 0;
        for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
            rootDeviceEnvironment->setHwInfo(defaultHwInfo.get());
            rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMockCustom(*rootDeviceEnvironment)));
            rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i);
            i++;
        }
        mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[0].probedSize = 8 * GB;
        regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
        regionInfo[1].probedSize = 16 * GB;
        mock->memoryInfo.reset(new MockedMemoryInfo(regionInfo));
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);
        memoryManager.reset(new TestedDrmMemoryManager(enableLocalMem, false, false, *executionEnvironment));

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

class DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation : public DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    void SetUp() {
        DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::SetUp(true);
    }
    void TearDown() {
        DrmMemoryManagerFixtureWithoutQuietIoctlExpectation::TearDown();
    }
};
} // namespace NEO
