/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include "opencl/source/platform/platform.h"
#include "test.h"

using namespace NEO;

class DrmCommandStreamMMTest : public ::testing::Test {
};

HWTEST_F(DrmCommandStreamMMTest, GivenForcePinThenMemoryManagerCreatesPinBb) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(true);

    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->pinBBs[0]);
}

HWTEST_F(DrmCommandStreamMMTest, givenForcePinDisabledWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(false);

    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->pinBBs[0]);
}

HWTEST_F(DrmCommandStreamMMTest, givenExecutionEnvironmentWithMoreThanOneRootDeviceEnvWhenCreatingDrmMemoryManagerThenCreateAsManyPinBBs) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(2);

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    }

    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        EXPECT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    }
}
