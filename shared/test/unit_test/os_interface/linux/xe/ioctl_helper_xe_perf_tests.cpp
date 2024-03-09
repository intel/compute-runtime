/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/xe/ioctl_helper_xe_tests.h"

using namespace NEO;

TEST(IoctlHelperXeTest, whenCallingGetEuStallPropertiesThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    std::array<uint64_t, 12u> properties = {};
    EXPECT_FALSE(xeIoctlHelper.get()->getEuStallProperties(properties, 0x101, 0x102, 0x103, 1, 20u));
}

TEST(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    std::array<uint64_t, 12u> properties = {};
    EXPECT_FALSE(xeIoctlHelper.get()->perfOpenEuStallStream(0u, properties, &invalidFd));
}

TEST(IoctlHelperXeTest, whenCallingPerfDisableEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    EXPECT_FALSE(xeIoctlHelper.get()->perfDisableEuStallStream(&invalidFd));
}
