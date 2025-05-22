/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

namespace NEO {

using DrmExternalSemaphoreTest = Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation>;

TEST_F(DrmExternalSemaphoreTest, givenNullOsInterfaceWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(nullptr, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(DrmExternalSemaphoreTest, givenInvalidLinuxSemaphoreTypeWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::OpaqueWin32, nullptr, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(DrmExternalSemaphoreTest, givenOpaqueFdSemaphoreTypeWhenCreateExternalSemaphoreIsCalledThenNonNullptrIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);
}

TEST_F(DrmExternalSemaphoreTest, givenIoctlFailsWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    mockDrm->failOnSyncObjFdToHandle = true;
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjFdToHandle, 0);

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjFdToHandle, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenIoctlFailsWhenEnqueueSignalIsCalledThenFalseIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    mockDrm->failOnSyncObjSignal = true;

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 0);
    auto result = externalSemaphore->enqueueSignal(&fenceValue);
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenIoctlFailsSemaphoreWhenEnqueueWaitIsCalledThenFalseIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    mockDrm->failOnSyncObjWait = true;

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 0);
    auto result = externalSemaphore->enqueueWait(&fenceValue);
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenOpaqueFdSemaphoreWhenEnqueueSignalIsCalledThenTrueIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 0);
    auto result = externalSemaphore->enqueueSignal(&fenceValue);
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenOpaqueFdSemaphoreWhenEnqueueWaitIsCalledThenTrueIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 0);
    auto result = externalSemaphore->enqueueWait(&fenceValue);
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdIoctlFailsWhenEnqueueSignalIsCalledThenFalseIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    mockDrm->failOnSyncObjTimelineSignal = true;

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 0);
    auto result = externalSemaphore->enqueueSignal(&fenceValue);
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdIoctlFailsSemaphoreWhenEnqueueWaitIsCalledThenFalseIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    mockDrm->failOnSyncObjTimelineWait = true;

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 0);
    auto result = externalSemaphore->enqueueWait(&fenceValue);
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdSemaphoreWhenEnqueueSignalIsCalledThenTrueIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 0);
    auto result = externalSemaphore->enqueueSignal(&fenceValue);
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdSemaphoreWhenEnqueueWaitIsCalledThenTrueIsReturned) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 0);
    auto result = externalSemaphore->enqueueWait(&fenceValue);
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 1);
}

} // namespace NEO