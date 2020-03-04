/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_interface.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/os_interface/linux/drm_command_stream.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/linux/mock_drm_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture.h"
#include "test.h"

using namespace NEO;

class DrmCommandStreamMMTest : public ::testing::Test {
};

HWTEST_F(DrmCommandStreamMMTest, MMwithPinBB) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(true);

    auto drm = new DrmMockCustom();
    MockExecutionEnvironment executionEnvironment;

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(drm);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->pinBBs[0]);
}

HWTEST_F(DrmCommandStreamMMTest, givenForcePinDisabledWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(false);

    auto drm = new DrmMockCustom();
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(*platformDevices);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(drm);

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, gemCloseWorkerMode::gemCloseWorkerInactive);
    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->pinBBs[0]);
}

HWTEST_F(DrmCommandStreamMMTest, givenExecutionEnvironmentWithMoreThanOneRootDeviceEnvWhenCreatingDrmMemoryManagerThenCreateAsManyPinBBs) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(2);

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(*platformDevices);
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->setDrm(new DrmMockCustom());
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();
    }

    auto memoryManager = new TestedDrmMemoryManager(false, true, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        EXPECT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    }
}
