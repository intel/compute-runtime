/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"
#include "test.h"

using namespace OCLRT;

class DrmCommandStreamMMTest : public ::testing::Test {
};

HWTEST_F(DrmCommandStreamMMTest, MMwithPinBB) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.EnableForcePin.set(true);

        std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom());
        ASSERT_NE(nullptr, mock);

        DrmCommandStreamReceiver<FamilyType> csr(*platformDevices[0], mock.get(), gemCloseWorkerMode::gemCloseWorkerInactive);

        auto mm = (DrmMemoryManager *)csr.createMemoryManager(false);
        ASSERT_NE(nullptr, mm);
        EXPECT_NE(nullptr, mm->getPinBB());
        csr.setMemoryManager(nullptr);

        delete mm;
    }
}

HWTEST_F(DrmCommandStreamMMTest, givenForcePinDisabledWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.EnableForcePin.set(false);

        std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom());
        ASSERT_NE(nullptr, mock);

        DrmCommandStreamReceiver<FamilyType> csr(*platformDevices[0], mock.get(), gemCloseWorkerMode::gemCloseWorkerInactive);

        auto mm = (DrmMemoryManager *)csr.createMemoryManager(false);
        csr.setMemoryManager(nullptr);

        ASSERT_NE(nullptr, mm);
        EXPECT_NE(nullptr, mm->getPinBB());

        delete mm;
    }
}
