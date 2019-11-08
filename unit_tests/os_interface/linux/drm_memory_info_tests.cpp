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

struct MemoryInfoImpl : public NEO::MemoryInfo {
    MemoryInfoImpl() {}
    ~MemoryInfoImpl() override{};
};

TEST(DrmTest, whenQueryingEngineInfoThenEngineInfoIsNotCreatedAndNoIoctlsAreCalled) {
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    EXPECT_TRUE(drm->queryEngineInfo());

    EXPECT_EQ(nullptr, drm->engineInfo.get());
    EXPECT_EQ(0u, drm->ioctlCallsCount);
}

TEST(DrmTest, whenQueryingMemoryInfoThenMemoryInfoIsNotCreatedAndNoIoctlsAreCalled) {
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    EXPECT_TRUE(drm->queryMemoryInfo());

    EXPECT_EQ(nullptr, drm->memoryInfo.get());
    EXPECT_EQ(0u, drm->ioctlCallsCount);
}

TEST(DrmTest, givenMemoryInfoWhenGetMemoryInfoIsCalledThenValidPtrIsReturned) {
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    drm->memoryInfo.reset(new MemoryInfoImpl);

    EXPECT_EQ(drm->memoryInfo.get(), drm->getMemoryInfo());
}

TEST(MemoryInfo, givenMemoryInfoImplementationWhenDestructingThenDestructorIsCalled) {
    MemoryInfoImpl memoryInfoImpl;
}

TEST(MemoryInfo, givenMemoryRegionIdWhenGetMemoryTypeFromRegionAndGetInstanceFromRegionAreCalledThenMemoryTypeAndInstanceAreReturned) {
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    auto regionSmem = drm->createMemoryRegionId(0, 0);
    EXPECT_EQ(0u, drm->getMemoryTypeFromRegion(regionSmem));
    EXPECT_EQ(0u, drm->getInstanceFromRegion(regionSmem));

    auto regionLmem = drm->createMemoryRegionId(1, 0);
    EXPECT_EQ(1u, drm->getMemoryTypeFromRegion(regionLmem));
    EXPECT_EQ(0u, drm->getInstanceFromRegion(regionLmem));

    auto regionLmem1 = drm->createMemoryRegionId(1, 1);
    EXPECT_EQ(1u, drm->getMemoryTypeFromRegion(regionLmem1));
    EXPECT_EQ(1u, drm->getInstanceFromRegion(regionLmem1));

    auto regionLmem2 = drm->createMemoryRegionId(1, 2);
    EXPECT_EQ(1u, drm->getMemoryTypeFromRegion(regionLmem2));
    EXPECT_EQ(2u, drm->getInstanceFromRegion(regionLmem2));

    auto regionLmem3 = drm->createMemoryRegionId(1, 3);
    EXPECT_EQ(1u, drm->getMemoryTypeFromRegion(regionLmem3));
    EXPECT_EQ(3u, drm->getInstanceFromRegion(regionLmem3));
}
