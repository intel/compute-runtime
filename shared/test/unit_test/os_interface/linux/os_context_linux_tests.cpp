/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

TEST(OSContextLinux, givenReinitializeContextWhenContextIsInitThenContextIsStillIinitializedAfter) {
    MockExecutionEnvironment executionEnvironment;
    auto mock = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u, false);
    OsContextLinux osContext(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_NO_THROW(osContext.reInitializeContext());
    EXPECT_NO_THROW(osContext.ensureContextInitialized(false));
}

TEST(OSContextLinux, givenInitializeContextWhenContextCreateIoctlFailsThenContextNotInitialized) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = -1;
    EXPECT_EQ(-1, pDrm->createDrmContext(1, false, false));

    OsContextLinux osContext(*pDrm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_EQ(false, osContext.ensureContextInitialized(false));
    delete pDrm;
}

TEST(OSContextLinux, givenOsContextLinuxWhenQueryingForOfflineDumpContextIdThenCorrectValueIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    auto mock = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u, false);
    MockOsContextLinux osContext(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());

    osContext.drmContextIds.clear();
    osContext.drmContextIds.push_back(1u);
    osContext.drmContextIds.push_back(3u);
    osContext.drmContextIds.push_back(5u);

    const auto processId = 0xABCEDF;
    const uint64_t highBitsMask = 0xffffffff00000000;
    const uint64_t lowBitsMask = 0x00000000ffffffff;

    auto ctxId = osContext.getOfflineDumpContextId(0);
    EXPECT_EQ(ctxId & lowBitsMask, static_cast<uint64_t>(1u));
    EXPECT_EQ(ctxId & highBitsMask, static_cast<uint64_t>(processId) << 32);

    ctxId = osContext.getOfflineDumpContextId(1);
    EXPECT_EQ(ctxId & lowBitsMask, static_cast<uint64_t>(3u));
    EXPECT_EQ(ctxId & highBitsMask, static_cast<uint64_t>(processId) << 32);

    ctxId = osContext.getOfflineDumpContextId(2);
    EXPECT_EQ(ctxId & lowBitsMask, static_cast<uint64_t>(5u));
    EXPECT_EQ(ctxId & highBitsMask, static_cast<uint64_t>(processId) << 32);

    EXPECT_EQ(0u, osContext.getOfflineDumpContextId(10));
}

TEST(OSContextLinux, givenPerContextVmsAndBindNotCompleteWhenWaitForPagingFenceThenContextFenceIsPassedToWaitUserFenceIoctl) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.requirePerContextVM = true;

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    drm.pagingFence[0] = 26u;
    drm.fenceVal[0] = 31u;

    osContext.pagingFence[0] = 46u;
    osContext.fenceVal[0] = 51u;

    osContext.waitForPagingFence();

    EXPECT_EQ(1u, drm.waitUserFenceParams.size());
    EXPECT_EQ(0u, drm.waitUserFenceParams[0].ctxId);
    EXPECT_EQ(castToUint64(&osContext.pagingFence[0]), drm.waitUserFenceParams[0].address);
    EXPECT_EQ(drm.ioctlHelper->getWaitUserFenceSoftFlag(), drm.waitUserFenceParams[0].flags);
    EXPECT_EQ(osContext.fenceVal[0], drm.waitUserFenceParams[0].value);
    EXPECT_EQ(-1, drm.waitUserFenceParams[0].timeout);

    drm.requirePerContextVM = false;
    osContext.waitForPagingFence();

    EXPECT_EQ(castToUint64(&drm.pagingFence[0]), drm.waitUserFenceParams[1].address);
    EXPECT_EQ(drm.ioctlHelper->getWaitUserFenceSoftFlag(), drm.waitUserFenceParams[1].flags);
    EXPECT_EQ(drm.fenceVal[0], drm.waitUserFenceParams[1].value);
}

TEST(OSContextLinux, givenPerContextVmsAndBindCompleteWhenWaitForPagingFenceThenWaitUserFenceIoctlIsNotCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.requirePerContextVM = true;

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    osContext.pagingFence[0] = 3u;
    osContext.fenceVal[0] = 3u;

    osContext.waitForPagingFence();

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());
}

TEST(OSContextLinux, givenPerContextVmsAndBindNotCompleteWhenWaitForPagingFenceGivenFenceValThenContextFenceIsPassedToWaitUserFenceIoctl) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.requirePerContextVM = true;

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    drm.pagingFence[0] = 26u;

    osContext.pagingFence[0] = 46u;
    uint64_t fenceValToWait = 51u;

    osContext.waitForPagingFenceGivenFenceVal(fenceValToWait);

    EXPECT_EQ(1u, drm.waitUserFenceParams.size());
    EXPECT_EQ(0u, drm.waitUserFenceParams[0].ctxId);
    EXPECT_EQ(castToUint64(&osContext.pagingFence[0]), drm.waitUserFenceParams[0].address);
    EXPECT_EQ(drm.ioctlHelper->getWaitUserFenceSoftFlag(), drm.waitUserFenceParams[0].flags);
    EXPECT_EQ(fenceValToWait, drm.waitUserFenceParams[0].value);
    EXPECT_EQ(-1, drm.waitUserFenceParams[0].timeout);

    drm.requirePerContextVM = false;
    osContext.waitForPagingFenceGivenFenceVal(fenceValToWait);

    EXPECT_EQ(castToUint64(&drm.pagingFence[0]), drm.waitUserFenceParams[1].address);
    EXPECT_EQ(drm.ioctlHelper->getWaitUserFenceSoftFlag(), drm.waitUserFenceParams[1].flags);
    EXPECT_EQ(fenceValToWait, drm.waitUserFenceParams[1].value);
}

TEST(OSContextLinux, givenPerContextVmsAndBindCompleteWhenWaitForPagingFenceGivenFenceValThenWaitUserFenceIoctlIsNotCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.requirePerContextVM = true;

    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    osContext.pagingFence[0] = 3u;
    uint64_t fenceValToWait = 3u;

    osContext.waitForPagingFenceGivenFenceVal(fenceValToWait);

    EXPECT_EQ(0u, drm.waitUserFenceParams.size());
}
