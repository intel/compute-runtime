/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(DrmBindTest, givenBindAlreadyCompleteWhenWaitForBindThenWaitUserFenceIoctlIsNotCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    drm.pagingFence[0] = 31u;
    drm.fenceVal[0] = 31u;

    drm.waitForBind(0u);

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());

    drm.pagingFence[0] = 49u;
    drm.fenceVal[0] = 31u;

    drm.waitForBind(0u);

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());
}

TEST(DrmBindTest, givenBindNotCompleteWhenWaitForBindThenWaitUserFenceIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    drm.pagingFence[0] = 26u;
    drm.fenceVal[0] = 31u;

    drm.waitForBind(0u);

    EXPECT_EQ(1u, drm.waitUserFenceParams.size());
    EXPECT_EQ(0u, drm.waitUserFenceParams[0].ctxId);
    EXPECT_EQ(castToUint64(&drm.pagingFence[0]), drm.waitUserFenceParams[0].address);
    EXPECT_EQ(drm.ioctlHelper->getWaitUserFenceSoftFlag(), drm.waitUserFenceParams[0].flags);
    EXPECT_EQ(drm.fenceVal[0], drm.waitUserFenceParams[0].value);
    EXPECT_EQ(-1, drm.waitUserFenceParams[0].timeout);
}

TEST(DrmBindTest, givenBindAlreadyCompleteWhenwaitForBindGivenFenceValThenWaitUserFenceIoctlIsNotCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    drm.pagingFence[0] = 31u;
    uint64_t fenceValToWait = 31u;

    drm.waitForBindGivenFenceVal(0u, fenceValToWait);

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());

    drm.pagingFence[0] = 49u;

    drm.waitForBindGivenFenceVal(0u, fenceValToWait);

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());
}

TEST(DrmBindTest, givenBindNotCompleteWhenwaitForBindGivenFenceValThenWaitUserFenceIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    drm.pagingFence[0] = 26u;
    uint64_t fenceValToWait = 31u;

    drm.waitForBindGivenFenceVal(0u, fenceValToWait);

    EXPECT_EQ(1u, drm.waitUserFenceParams.size());
    EXPECT_EQ(0u, drm.waitUserFenceParams[0].ctxId);
    EXPECT_EQ(castToUint64(&drm.pagingFence[0]), drm.waitUserFenceParams[0].address);
    EXPECT_EQ(drm.ioctlHelper->getWaitUserFenceSoftFlag(), drm.waitUserFenceParams[0].flags);
    EXPECT_EQ(fenceValToWait, drm.waitUserFenceParams[0].value);
    EXPECT_EQ(-1, drm.waitUserFenceParams[0].timeout);
}

TEST(DrmBindTest, whenCheckingVmBindAvailabilityThenIoctlHelperSupportIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.callBaseIsVmBindAvailable = true;

    EXPECT_EQ(drm.isVmBindAvailable(), drm.getIoctlHelper()->isVmBindAvailable());
}

TEST(DrmBindTest, whenCheckingSetPairAvailabilityThenIoctlHelperSupportIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.callBaseIsSetPairAvailable = true;

    EXPECT_EQ(drm.isSetPairAvailable(), drm.getIoctlHelper()->isSetPairAvailable());
}

TEST(DrmBindTest, whenCheckingChunkingAvailabilityThenIoctlHelperSupportIsUsed) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.callBaseIsChunkingAvailable = true;

    EXPECT_EQ(drm.isChunkingAvailable(), drm.getIoctlHelper()->isChunkingAvailable());
}