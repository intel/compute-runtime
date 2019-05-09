/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenQueryingDrmThenNullIsReturnedAndNoIoctlIsCalled) {
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    EXPECT_EQ(nullptr, drm->query(1));
    EXPECT_EQ(0u, drm->ioctlCallsCount);
}

TEST(DrmTest, whenQueryingEngineInfoThenNoIoctlIsCalled) {
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_EQ(0u, drm->ioctlCallsCount);
}

TEST(EngineInfoTest, givenEngineInfoImplementationWhenDestructingThenDestructorIsCalled) {
    struct EngineInfoImpl : EngineInfo {
        ~EngineInfoImpl() override = default;
    };
    EngineInfoImpl engineInfo;
}
