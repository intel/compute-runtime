/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

struct MemoryInfoImpl : public NEO::MemoryInfo {
    MemoryInfoImpl() {}
    ~MemoryInfoImpl() override{};
    size_t getMemoryRegionSize(uint32_t memoryBank) override {
        return 0u;
    }
    uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override {
        return 0u;
    }
    uint32_t createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) override {
        return 0u;
    }
};

TEST(DrmTest, whenQueryingMemoryInfoThenMemoryInfoIsNotCreatedAndNoIoctlsAreCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, drm);

    EXPECT_TRUE(drm->queryMemoryInfo());

    EXPECT_EQ(nullptr, drm->memoryInfo.get());
    EXPECT_EQ(0u, drm->ioctlCallsCount);
}

TEST(DrmTest, givenMemoryInfoWhenGetMemoryInfoIsCalledThenValidPtrIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, drm);

    drm->memoryInfo.reset(new MemoryInfoImpl);

    EXPECT_EQ(drm->memoryInfo.get(), drm->getMemoryInfo());
}

TEST(MemoryInfo, givenMemoryInfoImplementationWhenDestructingThenDestructorIsCalled) {
    MemoryInfoImpl memoryInfoImpl;
}
