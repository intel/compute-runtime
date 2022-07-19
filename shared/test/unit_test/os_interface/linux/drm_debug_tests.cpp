/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

struct DrmDebugTest : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    }

    void TearDown() override {
    }

  protected:
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

TEST_F(DrmDebugTest, whenRegisterResourceClassesCalledThenTrueIsReturned) {
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    auto result = drmMock.registerResourceClasses();
    EXPECT_TRUE(result);
}

TEST_F(DrmDebugTest, whenRegisterResourceCalledThenImplementationIsEmpty) {
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    drmMock.ioctlCallsCount = 0;
    auto handle = drmMock.registerResource(DrmResourceClass::MaxSize, nullptr, 0);
    EXPECT_EQ(0u, handle);
    drmMock.unregisterResource(handle);
    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}

TEST_F(DrmDebugTest, whenRegisterIsaCookieCalledThenImplementationIsEmpty) {
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    drmMock.ioctlCallsCount = 0;
    const uint32_t isaHandle = 2;
    auto handle = drmMock.registerIsaCookie(isaHandle);
    EXPECT_EQ(0u, handle);
    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}

TEST_F(DrmDebugTest, whenCheckingContextDebugSupportThenNoIoctlIsCalled) {
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.ioctlCallsCount = 0;
    drmMock.checkContextDebugSupport();
    EXPECT_FALSE(drmMock.isContextDebugSupported());

    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}

TEST_F(DrmDebugTest, whenNotifyCommandQueueCreateDestroyAreCalledThenImplementationsAreEmpty) {
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.ioctlCallsCount = 0;

    auto handle = drmMock.notifyFirstCommandQueueCreated(nullptr, 0);
    EXPECT_EQ(0u, handle);
    EXPECT_EQ(0u, drmMock.ioctlCallsCount);

    drmMock.notifyLastCommandQueueDestroyed(0);
    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}
