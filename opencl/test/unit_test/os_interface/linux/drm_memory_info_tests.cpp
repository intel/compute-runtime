/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

struct MemoryInfoImpl : public NEO::MemoryInfo {
    MemoryInfoImpl() {}
    ~MemoryInfoImpl() override{};
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
