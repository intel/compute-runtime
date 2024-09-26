/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/global_teardown/global_platform_teardown.h"
#include "opencl/source/platform/platform.h"

using namespace NEO;

class GlobalPlatformTeardownTest : public ::testing::Test {

    void SetUp() override {
        tmpPlatforms = platformsImpl;
        platformsImpl = nullptr;
    }

    void TearDown() override {
        globalPlatformTeardown();
        wasPlatformTeardownCalled = false;
        platformsImpl = tmpPlatforms;
    }
    std::vector<std::unique_ptr<Platform>> *tmpPlatforms;
};

TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformSetupThenPlatformsAllocated) {
    globalPlatformSetup();
    EXPECT_NE(platformsImpl, nullptr);
}
TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformSetupThenWasTeardownCalledIsSetToFalse) {
    globalPlatformSetup();
    EXPECT_FALSE(wasPlatformTeardownCalled);
}
TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformTeardownThenPlatformsDestroyed) {
    globalPlatformSetup();
    globalPlatformTeardown();
    EXPECT_EQ(platformsImpl, nullptr);
}
TEST_F(GlobalPlatformTeardownTest, whenCallingPlatformTeardownThenWasTeardownCalledIsSetToTrue) {
    globalPlatformSetup();
    globalPlatformTeardown();
    EXPECT_TRUE(wasPlatformTeardownCalled);
}