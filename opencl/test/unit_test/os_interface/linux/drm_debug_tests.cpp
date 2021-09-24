/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenRegisterResourceClassesCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    auto result = drmMock.registerResourceClasses();
    EXPECT_FALSE(result);
}

TEST(DrmTest, whenRegisterResourceCalledThenImplementationIsEmpty) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    auto handle = drmMock.registerResource(Drm::ResourceClass::MaxSize, nullptr, 0);
    EXPECT_EQ(0u, handle);
    drmMock.unregisterResource(handle);
    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}

TEST(DrmTest, whenRegisterIsaCookieCalledThenImplementationIsEmpty) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    const uint32_t isaHandle = 2;
    auto handle = drmMock.registerIsaCookie(isaHandle);
    EXPECT_EQ(0u, handle);
    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}

TEST(DrmTest, WhenCheckingContextDebugSupportThenNoIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.checkContextDebugSupport();
    EXPECT_FALSE(drmMock.isContextDebugSupported());

    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}
