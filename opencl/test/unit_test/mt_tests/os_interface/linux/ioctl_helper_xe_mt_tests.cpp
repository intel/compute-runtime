/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/xe/mock_drm_xe.h"
#include "shared/test/common/os_interface/linux/xe/mock_ioctl_helper_xe.h"

#include "gtest/gtest.h"

#include <atomic>
#include <memory>
#include <thread>

using namespace NEO;

TEST(IoctlHelperXeMtTest, givenIoctlHelperXeWhenGemCloseIsCalledThenBindInfoIsProperlyHandled) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto drm = DrmMockXe::create(*executionEnvironment.rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);
    constexpr uint32_t maxThreads = 10;
    std::thread threads[maxThreads];
    constexpr uint32_t numHandlesPerThread = 10;

    auto totalNumHandles = maxThreads * numHandlesPerThread;

    for (auto i = 0u; i < totalNumHandles; i++) {
        xeIoctlHelper->updateBindInfo(i + 1);
        xeIoctlHelper->updateBindInfo(2 * totalNumHandles + i + 1);
    }
    EXPECT_EQ(2 * totalNumHandles, xeIoctlHelper->bindInfo.size());

    auto closeFunction = [&](uint32_t threadId) {
        for (auto i = 0u; i < numHandlesPerThread; i++) {
            GemClose close = {};
            close.userptr = threadId * numHandlesPerThread + i + 1;
            auto ret = xeIoctlHelper->ioctl(DrmIoctl::gemClose, &close);
            EXPECT_EQ(0, ret);
        }
    };
    for (uint32_t i = 0; i < maxThreads; i++) {
        threads[i] = std::thread(closeFunction, i);
    }
    for (size_t i = 0; i < maxThreads; i++) {
        threads[i].join();
    }
    EXPECT_EQ(totalNumHandles, xeIoctlHelper->bindInfo.size());
    for (auto i = 0u; i < totalNumHandles; i++) {
        EXPECT_EQ(2 * totalNumHandles + i + 1, xeIoctlHelper->bindInfo[i].userptr);
    }
}
