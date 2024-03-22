/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/xe/ioctl_helper_xe_tests.h"

using namespace NEO;

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetFlagsForVmBindThenExpectedValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    for (auto &bindCapture : ::testing::Bool()) {
        for (auto &bindImmediate : ::testing::Bool()) {
            for (auto &bindMakeResident : ::testing::Bool()) {
                for (auto &bindLockedMemory : ::testing::Bool()) {
                    auto flags = xeIoctlHelper->getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLockedMemory);
                    if (bindCapture) {
                        EXPECT_EQ(static_cast<uint32_t>(XE_NEO_BIND_CAPTURE_FLAG), (flags & XE_NEO_BIND_CAPTURE_FLAG));
                    }
                    if (bindImmediate) {
                        EXPECT_EQ(static_cast<uint32_t>(DRM_XE_VM_BIND_FLAG_IMMEDIATE), (flags & DRM_XE_VM_BIND_FLAG_IMMEDIATE));
                    }
                    if (bindMakeResident) {
                        EXPECT_EQ(static_cast<uint32_t>(XE_NEO_BIND_MAKERESIDENT_FLAG), (flags & XE_NEO_BIND_MAKERESIDENT_FLAG));
                    }
                    if (flags == 0) {
                        EXPECT_FALSE(bindCapture);
                        EXPECT_FALSE(bindImmediate);
                        EXPECT_FALSE(bindMakeResident);
                    }
                }
            }
        }
    }
}
