/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "hw_cmds.h"
#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "gmock/gmock.h"
#include <memory>

using namespace OCLRT;

TEST(Device_GetCaps, givenForceClGlSharingWhenCapsAreCreatedThenDeviceReporstClGlSharingExtension) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.AddClGlSharing.set(true);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();

        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_gl_sharing ")));
        DebugManager.flags.AddClGlSharing.set(false);
    }
}
