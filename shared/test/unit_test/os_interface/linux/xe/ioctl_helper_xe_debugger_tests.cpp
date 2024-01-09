/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

using namespace NEO;

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetIoctForDebuggerThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    auto verifyIoctlRequestValue = [&xeIoctlHelper](auto value, DrmIoctl drmIoctl) {
        EXPECT_EQ(xeIoctlHelper->getIoctlRequestValue(drmIoctl), static_cast<unsigned int>(value));
    };
    auto verifyIoctlString = [&xeIoctlHelper](DrmIoctl drmIoctl, const char *string) {
        EXPECT_STREQ(string, xeIoctlHelper->getIoctlString(drmIoctl).c_str());
    };

    verifyIoctlString(DrmIoctl::debuggerOpen, "DRM_IOCTL_XE_EUDEBUG_CONNECT");

    verifyIoctlRequestValue(DRM_IOCTL_XE_EUDEBUG_CONNECT, DrmIoctl::debuggerOpen);
}