/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/memory_info.h"
#include "unit_tests/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenQueryingMemoryInfoThenMemoryInfoIsNotCreatedAndNoIoctlIsCalled) {
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    drm->queryMemoryInfo();

    EXPECT_EQ(nullptr, drm->memoryInfo.get());
    EXPECT_EQ(0u, drm->ioctlCallsCount);
}

TEST(MemoryInfo, givenMemoryInfoImplementationWhenDestructingThenDestructorIsCalled) {
    struct MemoryInfoImpl : public NEO::MemoryInfo {
        MemoryInfoImpl() {}
        ~MemoryInfoImpl() override{};
    };

    MemoryInfoImpl memoryInfoImpl;
}
