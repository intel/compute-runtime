/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

namespace NEO {

class DrmMemoryManagerFixtureImpl : public DrmMemoryManagerFixture {
  public:
    DrmMockCustom *mockExp;

    template <typename GfxFamily>
    void setUpT() {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = false;

        DebugManagerStateRestore restore;
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));
        MemoryManagementFixture::setUp();
        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        mockExp = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]).release();
        DrmMemoryManagerFixture::setUpT<GfxFamily>(mockExp, true);
    }

    template <typename GfxFamily>
    void tearDownT() {
        mockExp->testIoctls();
        DrmMemoryManagerFixture::tearDownT<GfxFamily>();
    }

    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};

class DrmMemoryManagerLocalMemoryTest : public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex];
        rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());

        mock = createDrm(rootDeviceEnvironment);
        ioctlHelper = static_cast<MockIoctlHelperWithCapture *>(mock->getIoctlHelper());
        setUpMemoryInfo();

        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        rootDeviceEnvironment.memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

        memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
        ioctlHelper->createGemExtCalls.clear();
    }

    virtual void setUpMemoryInfo() {
        mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    }

    static DrmMock *createDrm(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto drm = new DrmMock(rootDeviceEnvironment);
        drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);
        return drm;
    }

    void setUpSingleDeviceMemoryRegion(DrmMock &drm) {
        auto drmIoctlHelper = drm.getIoctlHelper();
        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {static_cast<uint16_t>(drmIoctlHelper->getDrmParamValue(DrmParam::memoryClassSystem)), 0};
        regionInfo[1].region = {static_cast<uint16_t>(drmIoctlHelper->getDrmParamValue(DrmParam::memoryClassDevice)), 0};
        drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
        drm.ioctlCallsCount = 0;
        static_cast<MockIoctlHelperWithCapture *>(drmIoctlHelper)->createGemExtCalls.clear();
    }

    DrmMock *createDrmForRootDevice(uint32_t index, bool withMemoryInfo) {
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[index];
        rootDeviceEnvironment.initGmm();
        auto drm = createDrm(rootDeviceEnvironment);
        if (withMemoryInfo) {
            setUpSingleDeviceMemoryRegion(*drm);
        } else {
            drm->memoryInfo.reset(nullptr);
            drm->ioctlCallsCount = 0;
        }
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        rootDeviceEnvironment.memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
        rootDeviceEnvironment.initGmm();
        return drm;
    }

    DrmMock *getDrm(uint32_t index) {
        return static_cast<DrmMock *>(executionEnvironment->rootDeviceEnvironments[index]->osInterface->getDriverModel()->as<Drm>());
    }

    static inline uint32_t munmapCalledCount = 0u;

  protected:
    DebugManagerStateRestore restorer{};
    ExecutionEnvironment *executionEnvironment = nullptr;
    DrmMock *mock = nullptr;
    MockIoctlHelperWithCapture *ioctlHelper = nullptr;
    std::unique_ptr<MockDevice> device;
    TestedDrmMemoryManager *memoryManager = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

class DrmMemoryManagerLocalMemoryMultiTileTest : public DrmMemoryManagerLocalMemoryTest {
  public:
    void setUpMemoryInfo() override {
        mock->memoryInfo.reset(new MockExtendedMemoryInfo(*mock));
    }
};

using DrmMemoryManagerTestImpl = Test<DrmMemoryManagerFixtureImpl>;

} // namespace NEO
