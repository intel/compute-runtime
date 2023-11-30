/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

TEST(Device_GetCaps, givenForceClGlSharingWhenCapsAreCreatedThenDeviceReportsClGlSharingExtension) {
    DebugManagerStateRestore dbgRestorer;
    {
        debugManager.flags.AddClGlSharing.set(true);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        const auto &caps = device->getDeviceInfo();

        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_gl_sharing ")));

        debugManager.flags.AddClGlSharing.set(false);
    }
}
