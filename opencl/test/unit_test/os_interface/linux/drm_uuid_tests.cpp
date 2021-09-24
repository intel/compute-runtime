/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmUuidTest, GivenDrmWhenGeneratingUUIDThenCorrectStringsAreReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto uuid1 = drm.generateUUID();
    auto uuid2 = drm.generateUUID();

    std::string uuidff;
    for (int i = 0; i < 0xff - 2; i++) {
        uuidff = drm.generateUUID();
    }

    EXPECT_STREQ("00000000-0000-0000-0000-000000000001", uuid1.c_str());
    EXPECT_STREQ("00000000-0000-0000-0000-000000000002", uuid2.c_str());
    EXPECT_STREQ("00000000-0000-0000-0000-0000000000ff", uuidff.c_str());
}

TEST(DrmUuidTest, GivenDrmWhenGeneratingElfUUIDThenCorrectStringsAreReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    char data[] = "abc";
    auto uuid1 = drm.generateElfUUID(static_cast<const void *>(data));

    EXPECT_STREQ("00000000-0000-0000-0000-000000000001", uuid1.c_str());
}
