/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info_impl.h"

#include "opencl/source/memory_manager/memory_banks.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock_dg1.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryInfo, givenMemoryRegionQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreatedWithRegions) {
    auto drm = std::make_unique<DrmMockDg1>();
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto memoryInfo = static_cast<MemoryInfoImpl *>(drm->getMemoryInfo());

    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(2u, memoryInfo->regions.size());
}

TEST(MemoryInfo, givenMemoryRegionQueryNotSupportedWhenQueryingMemoryInfoThenMemoryInfoIsNotCreated) {
    auto drm = std::make_unique<DrmMockDg1>();
    ASSERT_NE(nullptr, drm);

    drm->i915QuerySuccessCount = 0;
    drm->queryMemoryInfo();

    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(MemoryInfo, givenMemoryRegionQueryWhenQueryingFailsThenMemoryInfoIsNotCreated) {
    auto drm = std::make_unique<DrmMockDg1>();
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryRegionInfoSuccessCount = 0;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmMockDg1>();
    ASSERT_NE(nullptr, drm);
    drm->i915QuerySuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmMockDg1>();
    ASSERT_NE(nullptr, drm);
    drm->queryMemoryRegionInfoSuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfoImpl>(regionInfo, 2);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank);
    EXPECT_EQ(regionInfo[0].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[0].region.memory_instance, regionClassAndInstance.memory_instance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::Bank0);
    EXPECT_EQ(regionInfo[1].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[1].region.memory_instance, regionClassAndInstance.memory_instance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::Bank0);
    EXPECT_EQ(16 * GB, regionSize);
}

TEST(MemoryInfo, givenMemoryInfoWithoutRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnInvalidMemoryRegion) {
    drm_i915_memory_region_info regionInfo = {};

    auto memoryInfo = std::make_unique<MemoryInfoImpl>(&regionInfo, 0);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank);
    EXPECT_EQ(MemoryInfoImpl::invalidMemoryRegion(), regionClassAndInstance.memory_class);
    EXPECT_EQ(MemoryInfoImpl::invalidMemoryRegion(), regionClassAndInstance.memory_instance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(0 * GB, regionSize);
}

TEST(MemoryInfo, givenMemoryRegionIdWhenGetMemoryTypeFromRegionAndGetInstanceFromRegionAreCalledThenMemoryTypeAndInstanceAreReturned) {
    auto drm = std::make_unique<DrmMockDg1>();
    EXPECT_NE(nullptr, drm);

    auto regionSmem = drm->createMemoryRegionId(0, 0);
    EXPECT_EQ(0u, drm->getMemoryTypeFromRegion(regionSmem));
    EXPECT_EQ(0u, drm->getMemoryInstanceFromRegion(regionSmem));

    auto regionLmem = drm->createMemoryRegionId(1, 0);
    EXPECT_EQ(1u, drm->getMemoryTypeFromRegion(regionLmem));
    EXPECT_EQ(0u, drm->getMemoryInstanceFromRegion(regionLmem));
}
