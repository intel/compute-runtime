/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/os_interface/linux/xe/mock_drm_xe.h"
#include "shared/test/common/os_interface/linux/xe/xe_config_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using IoctlHelperXeTest = Test<XeConfigFixture>;

TEST_F(IoctlHelperXeTest, whenCallingGetEuStallPropertiesThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    std::array<uint64_t, 12u> properties = {};
    EXPECT_FALSE(xeIoctlHelper.get()->getEuStallProperties(properties, 0x101, 0x102, 0x103, 1, 20u));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    std::array<uint64_t, 12u> properties = {};
    EXPECT_FALSE(xeIoctlHelper.get()->perfOpenEuStallStream(0u, properties, &invalidFd));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfDisableEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    EXPECT_FALSE(xeIoctlHelper.get()->perfDisableEuStallStream(&invalidFd));
}

TEST_F(IoctlHelperXeTest, whenCallingGetIoctlRequestValuePerfOpenThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::perfOpen));
}

TEST_F(IoctlHelperXeTest, whenCallingGetIoctlRequestValuePerfEnableThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::perfEnable));
}

TEST_F(IoctlHelperXeTest, whenCallingGetIoctlRequestValuePerfDisableThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::perfDisable));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenIoctlWithInvalidValuesThenZeroisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0, xeIoctlHelper.get()->perfOpenIoctl(DrmIoctl::perfOpen, nullptr));
}

TEST_F(IoctlHelperXeTest, whenCallingGetIoctlRequestValueWithInvalidValueThenErrorReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0u, xeIoctlHelper.get()->getIoctlRequestValuePerf(DrmIoctl::version));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenIoctlThenProperValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(0, xeIoctlHelper.get()->ioctl(DrmIoctl::perfOpen, nullptr));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfDisableIoctlThenProperValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    EXPECT_EQ(-1, xeIoctlHelper.get()->ioctl(invalidFd, DrmIoctl::perfDisable, nullptr));
}

TEST_F(IoctlHelperXeTest, whenCallingIsEuStallSupportedThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_FALSE(xeIoctlHelper.get()->isEuStallSupported());
}
