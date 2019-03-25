/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

using namespace OCLRT;

class DrmCommandStreamMMTest : public ::testing::Test {
};

HWTEST_F(DrmCommandStreamMMTest, MMwithPinBB) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(true);

    DrmMockCustom mock;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);

    executionEnvironment.osInterface = std::make_unique<OSInterface>();
    executionEnvironment.osInterface->get()->setDrm(&mock);

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = new DrmMemoryManager(gemCloseWorkerInactive,
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
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);

    executionEnvironment.osInterface = std::make_unique<OSInterface>();
    executionEnvironment.osInterface->get()->setDrm(&mock);

    DrmCommandStreamReceiver<FamilyType> csr(executionEnvironment, gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = new DrmMemoryManager(gemCloseWorkerInactive,
                                              DebugManager.flags.EnableForcePin.get(),
                                              true,
                                              executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}
