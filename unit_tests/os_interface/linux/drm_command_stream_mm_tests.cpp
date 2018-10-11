/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"
#include "test.h"

using namespace OCLRT;

class DrmCommandStreamMMTest : public ::testing::Test {
};

HWTEST_F(DrmCommandStreamMMTest, MMwithPinBB) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(true);

    DrmMockCustom mock;
    ExecutionEnvironment executionEnvironment;

    executionEnvironment.osInterface = std::make_unique<OSInterface>();
    executionEnvironment.osInterface->get()->setDrm(&mock);

    DrmCommandStreamReceiver<FamilyType> csr(*platformDevices[0], executionEnvironment,
                                             gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = static_cast<DrmMemoryManager *>(csr.createMemoryManager(false, false));
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}

HWTEST_F(DrmCommandStreamMMTest, givenForcePinDisabledWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableForcePin.set(false);

    DrmMockCustom mock;
    ExecutionEnvironment executionEnvironment;

    executionEnvironment.osInterface = std::make_unique<OSInterface>();
    executionEnvironment.osInterface->get()->setDrm(&mock);

    DrmCommandStreamReceiver<FamilyType> csr(*platformDevices[0], executionEnvironment,
                                             gemCloseWorkerMode::gemCloseWorkerInactive);

    auto memoryManager = static_cast<DrmMemoryManager *>(csr.createMemoryManager(false, false));
    executionEnvironment.memoryManager.reset(memoryManager);
    ASSERT_NE(nullptr, memoryManager);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}
