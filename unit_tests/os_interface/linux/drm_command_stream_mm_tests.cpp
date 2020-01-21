/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/linux/os_interface.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/drm_memory_operations_handler.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

using namespace NEO;

class DrmCommandStreamMMTest : public ::testing::Test {
};

HWTEST_F(DrmCommandStreamMMTest, MMwithPinBB) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(true);

    DrmMockCustom mock;
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(&mock);
    executionEnvironment.prepareRootDeviceEnvironments(1u);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive,
                                              DebugManager.flags.EnableForcePin.get(),
                                              true,
                                              executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}

HWTEST_F(DrmCommandStreamMMTest, givenForcePinDisabledWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(false);

    DrmMockCustom mock;
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(&mock);

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive,
                                              DebugManager.flags.EnableForcePin.get(),
                                              true,
                                              executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}
