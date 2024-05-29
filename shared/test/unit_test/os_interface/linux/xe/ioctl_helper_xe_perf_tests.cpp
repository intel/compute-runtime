/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/unit_test/os_interface/linux/xe/ioctl_helper_xe_tests.h"

using namespace NEO;

TEST(IoctlHelperXeTest, whenCallingGetEuStallPropertiesThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    std::array<uint64_t, 12u> properties = {};
    EXPECT_FALSE(xeIoctlHelper.get()->getEuStallProperties(properties, 0x101, 0x102, 0x103, 1, 20u));
}

TEST(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    std::array<uint64_t, 12u> properties = {};
    EXPECT_FALSE(xeIoctlHelper.get()->perfOpenEuStallStream(0u, properties, &invalidFd));
}

TEST(IoctlHelperXeTest, whenCallingPerfDisableEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    EXPECT_FALSE(xeIoctlHelper.get()->perfDisableEuStallStream(&invalidFd));
}

TEST(IoctlHelperXeTest, whenCallingGetIoctlRequestValuePerfOpenThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::perfOpen));
}

TEST(IoctlHelperXeTest, whenCallingGetIoctlRequestValuePerfEnableThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::perfEnable));
}

TEST(IoctlHelperXeTest, whenCallingGetIoctlRequestValuePerfDisableThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::perfDisable));
}

TEST(IoctlHelperXeTest, whenCallingPerfOpenIoctlWithInvalidValuesThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0, xeIoctlHelper.get()->perfOpenIoctl(DrmIoctl::perfOpen, nullptr));
}

TEST(IoctlHelperXeTest, whenCallingGetIoctlRequestValueWithInvalidValueThenErrorReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::version));
}

TEST(IoctlHelperXeTest, whenCallingPerfOpenIoctlThenProperValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0, xeIoctlHelper.get()->ioctl(DrmIoctl::perfOpen, nullptr));
}

TEST(IoctlHelperXeTest, whenCallingPerfDisableIoctlThenProperValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    EXPECT_EQ(0, xeIoctlHelper.get()->ioctl(invalidFd, DrmIoctl::perfDisable, nullptr));
}
