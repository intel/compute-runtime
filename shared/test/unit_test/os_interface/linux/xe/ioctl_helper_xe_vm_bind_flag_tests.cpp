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
                    for (auto &readOnlyResource : ::testing::Bool()) {
                        auto flags = xeIoctlHelper->getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLockedMemory, readOnlyResource);
                        if (bindCapture) {
                            EXPECT_EQ(static_cast<uint32_t>(DRM_XE_VM_BIND_FLAG_DUMPABLE), (flags & DRM_XE_VM_BIND_FLAG_DUMPABLE));
                        }
                        if (flags == 0) {
                            EXPECT_FALSE(bindCapture);
                        }
                    }
                }
            }
        }
    }
}
