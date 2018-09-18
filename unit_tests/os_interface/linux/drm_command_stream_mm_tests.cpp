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
    {
        ExecutionEnvironment executionEnvironment;
        DebugManager.flags.EnableForcePin.set(true);

        std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom());
        ASSERT_NE(nullptr, mock);

        executionEnvironment.osInterface = std::make_unique<OSInterface>();
        executionEnvironment.osInterface->get()->setDrm(mock.get());

        DrmCommandStreamReceiver<FamilyType> csr(*platformDevices[0], executionEnvironment,
                                                 gemCloseWorkerMode::gemCloseWorkerInactive);

        auto mm = (DrmMemoryManager *)csr.createMemoryManager(false, false);
        ASSERT_NE(nullptr, mm);
        EXPECT_NE(nullptr, mm->getPinBB());
        csr.setMemoryManager(nullptr);

        delete mm;
    }
}

HWTEST_F(DrmCommandStreamMMTest, givenForcePinDisabledWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    {
        ExecutionEnvironment executionEnvironment;
        DebugManager.flags.EnableForcePin.set(false);

        std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom());
        ASSERT_NE(nullptr, mock);

        executionEnvironment.osInterface = std::make_unique<OSInterface>();
        executionEnvironment.osInterface->get()->setDrm(mock.get());

        DrmCommandStreamReceiver<FamilyType> csr(*platformDevices[0], executionEnvironment,
                                                 gemCloseWorkerMode::gemCloseWorkerInactive);

        auto mm = (DrmMemoryManager *)csr.createMemoryManager(false, false);
        csr.setMemoryManager(nullptr);

        ASSERT_NE(nullptr, mm);
        EXPECT_NE(nullptr, mm->getPinBB());

        delete mm;
    }
}
