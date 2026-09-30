/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/linux/external_semaphore_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <array>
#include <limits>
#include <vector>

namespace NEO {

struct DrmExternalSemaphoreTest : public Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation> {
    void SetUp() override {
        Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation>::SetUp();
        osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
        mockDrm = static_cast<DrmMockCustom *>(osInterface->getDriverModel()->as<Drm>());
    }

    static std::array<ExternalSemaphoreOperation, 1> singleOperation(const ExternalSemaphore &semaphore, uint64_t fenceValue) {
        return {{{&semaphore, fenceValue}}};
    }

    OSInterface *osInterface = nullptr;
    DrmMockCustom *mockDrm = nullptr;
};

TEST_F(DrmExternalSemaphoreTest, givenNullOsInterfaceWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(nullptr, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr, importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::unsupported, importResult);
}

TEST_F(DrmExternalSemaphoreTest, givenInvalidLinuxSemaphoreTypeWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueWin32, nullptr, 0u, nullptr, importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::unsupported, importResult);
}

TEST_F(DrmExternalSemaphoreTest, givenOpaqueFdSemaphoreTypeWhenCreateExternalSemaphoreIsCalledThenNonNullptrIsReturned) {
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::unsupported;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr, importResult);
    EXPECT_NE(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::success, importResult);
}

TEST_F(DrmExternalSemaphoreTest, givenSemaphoreWithBaseFenceValueAcquisitionWhenAcquireWaitFenceValueIsCalledThenPassedValueIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    EXPECT_EQ(0u, externalSemaphore->acquireWaitFenceValue(0u));
    EXPECT_EQ(123u, externalSemaphore->acquireWaitFenceValue(123u));
    EXPECT_EQ(123u, externalSemaphore->acquireWaitFenceValue(123u));
}

TEST_F(DrmExternalSemaphoreTest, givenSemaphoreWithBaseFenceValueAcquisitionWhenAcquireSignalFenceValueIsCalledThenPassedValueIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    EXPECT_EQ(0u, externalSemaphore->acquireSignalFenceValue(0u));
    EXPECT_EQ(321u, externalSemaphore->acquireSignalFenceValue(321u));
    EXPECT_EQ(321u, externalSemaphore->acquireSignalFenceValue(321u));
}

TEST_F(DrmExternalSemaphoreTest, givenIoctlFailsWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    mockDrm->failOnSyncObjFdToHandle = true;
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjFdToHandle, 0);

    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr, importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::invalidResource, importResult);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjFdToHandle, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenIoctlFailsWhenSignalExternalSemaphoresFromCpuIsCalledThenFalseIsReturned) {
    mockDrm->failOnSyncObjSignal = true;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 0);
    auto result = mockDrm->signalExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenIoctlFailsSemaphoreWhenWaitExternalSemaphoresFromCpuIsCalledThenFalseIsReturned) {
    mockDrm->failOnSyncObjWait = true;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 0);
    auto result = mockDrm->waitExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenOpaqueFdSemaphoreWhenSignalExternalSemaphoresFromCpuIsCalledThenTrueIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 0);
    auto result = mockDrm->signalExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenOpaqueFdSemaphoreWhenWaitExternalSemaphoresFromCpuIsCalledThenTrueIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 0);
    auto result = mockDrm->waitExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdIoctlFailsWhenSignalExternalSemaphoresFromCpuIsCalledThenFalseIsReturned) {
    mockDrm->failOnSyncObjTimelineSignal = true;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 0);
    auto result = mockDrm->signalExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdIoctlFailsSemaphoreWhenWaitExternalSemaphoresFromCpuIsCalledThenFalseIsReturned) {
    mockDrm->failOnSyncObjTimelineWait = true;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 0);
    auto result = mockDrm->waitExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, false);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdSemaphoreWhenSignalExternalSemaphoresFromCpuIsCalledThenTrueIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 0);
    auto result = mockDrm->signalExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineFdSemaphoreWhenWaitExternalSemaphoresFromCpuIsCalledThenTrueIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 0);
    auto result = mockDrm->waitExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));
    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 1);
}

// ============================================================================
// NEO-17059: File descriptor ownership tests
// These tests verify that file descriptors are properly closed after import
// and sync handles are properly destroyed in destructor.
// ============================================================================

TEST_F(DrmExternalSemaphoreTest, givenValidFdWhenImportingExternalSemaphoreThenFdIsClosed) {
    // Setup: Track close() calls
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    const int testFd = 42;

    // Exercise: Create external semaphore with specific FD
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, testFd, nullptr);

    EXPECT_NE(externalSemaphore, nullptr);

    // Verify: FD should have been closed after successful import
    // The driver takes ownership of the FD and must close it
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineSemaphoreFdWhenImportingThenFdIsClosed) {
    // Setup: Track close() calls
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    const int testFd = 99;

    // Exercise: Create timeline semaphore with specific FD
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, testFd, nullptr);

    EXPECT_NE(externalSemaphore, nullptr);

    // Verify: FD should have been closed after successful import
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmExternalSemaphoreTest, givenImportFailsWhenCreatingExternalSemaphoreThenFdIsClosed) {
    mockDrm->failOnSyncObjFdToHandle = true;

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    const int testFd = 77;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, testFd, nullptr);

    EXPECT_EQ(externalSemaphore, nullptr);

    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmExternalSemaphoreTest, givenIoctlFailureWithNonZeroFdThenFdIsClosedBeforeReturning) {
    mockDrm->failOnSyncObjFdToHandle = true;

    for (int i = 0; i < 10; i++) {
        VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
        VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

        const int testFd = 100 + i;

        auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, testFd, nullptr);

        EXPECT_EQ(externalSemaphore, nullptr);
        EXPECT_EQ(1u, SysCalls::closeFuncCalled);
        EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    }
}

TEST_F(DrmExternalSemaphoreTest, givenValidSemaphoreWhenDestroyedThenSyncHandleIsDestroyed) {
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    // Exercise: Create and destroy semaphore
    {
        auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0, nullptr);
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs here
    }

    // Verify: syncObjDestroy should have been called
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenMultipleSemaphoresWhenDestroyedThenAllSyncHandlesAreDestroyed) {
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    const int numSemaphores = 5;

    // Exercise: Create and destroy multiple semaphores
    {
        std::vector<std::unique_ptr<ExternalSemaphore>> semaphores;
        for (int i = 0; i < numSemaphores; i++) {
            auto sem = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, i, nullptr);
            EXPECT_NE(sem, nullptr);
            semaphores.push_back(std::move(sem));
        }
        // All destructors run here
    }

    // Verify: syncObjDestroy should have been called for each semaphore
    EXPECT_EQ(numSemaphores, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenManyImportsAndDestroysWhenRunInLoopThenNoFdLeak) {
    // This test verifies that we don't leak file descriptors over many iterations
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    const int iterations = 100;

    // Exercise: Import and destroy many semaphores
    for (int i = 0; i < iterations; i++) {
        auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, i, nullptr);
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs each iteration
    }

    // Verify: Every import should have closed its FD
    EXPECT_EQ(static_cast<uint32_t>(iterations - 1), SysCalls::closeFuncCalled);
}

TEST_F(DrmExternalSemaphoreTest, givenSyncObjDestroyFailsWhenDestroyingSemaphoreThenNoException) {
    mockDrm->failOnSyncObjDestroy = true;

    // Exercise: Create semaphore, then destroy - should not throw even if destroy fails
    EXPECT_NO_THROW({
        auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0, nullptr);
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs - syncObjDestroy will fail but should not throw
    });

    // Verify: Destroy was attempted
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenSemaphoreWithZeroSyncHandleWhenDestroyedThenSyncObjDestroyIsNotCalled) {
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    // Exercise: Create semaphore without importing (syncHandle remains 0), then destroy
    {
        auto externalSemaphore = ExternalSemaphoreLinux::create(osInterface);
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs here - should early return due to syncHandle == 0
    }

    // Verify: syncObjDestroy should NOT have been called (early return)
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenSemaphoreWithNullOsInterfaceWhenDestroyedThenSyncObjDestroyIsNotCalled) {
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    // Exercise: Create valid semaphore with imported handle, then set osInterface to nullptr
    {
        auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0, nullptr);
        EXPECT_NE(externalSemaphore, nullptr);

        // Simulate osInterface becoming invalid (e.g., during shutdown race conditions)
        externalSemaphore->osInterface = nullptr;

        // Destructor runs here - should early return due to osInterface == nullptr
    }

    // Verify: syncObjDestroy should NOT have been called (early return prevents crash)
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenExternalSemaphoreWhenWaitExternalSemaphoresFromCpuIsCalledThenSyncObjWaitIsBlocking) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 0u;
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjWait);

    auto result = mockDrm->waitExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));

    EXPECT_TRUE(result);
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjWait);
    EXPECT_EQ(std::numeric_limits<int64_t>::max(), mockDrm->syncObjWaitTimeoutNs);
    EXPECT_EQ(0x2u, mockDrm->syncObjWaitFlags & 0x2u);
}

TEST_F(DrmExternalSemaphoreTest, givenExternalSemaphoreWhenWaitExternalSemaphoresFromCpuIsCalledOnTimelineSemaphoreThenSyncObjTimelineWaitIsBlocking) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 1u;
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjTimelineWait);

    auto result = mockDrm->waitExternalSemaphoresFromCpu(singleOperation(*externalSemaphore, fenceValue));

    EXPECT_TRUE(result);
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjTimelineWait);
    EXPECT_EQ(std::numeric_limits<int64_t>::max(), mockDrm->syncObjTimelineWaitTimeoutNs);
    EXPECT_EQ(0x2u, mockDrm->syncObjTimelineWaitFlags & 0x2u);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineAndBinarySemaphoresWhenWaitExternalSemaphoresFromCpuIsCalledThenSingleWaitAllIoctlIsIssuedPerSemaphoreKind) {
    auto timeline0 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    auto binary0 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    auto timeline1 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    auto binary1 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);

    const ExternalSemaphoreOperation waits[] = {{timeline0.get(), 7u}, {binary0.get(), 0u}, {timeline1.get(), 9u}, {binary1.get(), 0u}};
    EXPECT_TRUE(mockDrm->waitExternalSemaphoresFromCpu(waits));

    constexpr uint32_t expectedFlags = 0x3u; // DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL | DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT

    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjTimelineWait);
    EXPECT_EQ((std::vector<uint64_t>{7u, 9u}), mockDrm->syncObjTimelineWaitPoints);
    EXPECT_EQ(expectedFlags, mockDrm->syncObjTimelineWaitFlags);

    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjWait);
    EXPECT_EQ(2u, mockDrm->syncObjWaitCountHandles);
    EXPECT_EQ(expectedFlags, mockDrm->syncObjWaitFlags);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineAndBinarySemaphoresWhenSignalExternalSemaphoresFromCpuIsCalledThenSingleIoctlIsIssuedPerSemaphoreKind) {
    auto timeline0 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    auto binary0 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    auto timeline1 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    auto binary1 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);

    const ExternalSemaphoreOperation signals[] = {{timeline0.get(), 7u}, {binary0.get(), 0u}, {timeline1.get(), 9u}, {binary1.get(), 0u}};
    EXPECT_TRUE(mockDrm->signalExternalSemaphoresFromCpu(signals));

    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjTimelineSignal);
    EXPECT_EQ((std::vector<uint64_t>{7u, 9u}), mockDrm->syncObjTimelineSignalPoints);

    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjSignal);
    EXPECT_EQ(2u, mockDrm->syncObjSignalCountHandles);
}

TEST_F(DrmExternalSemaphoreTest, givenOnlyBinarySemaphoresWhenWaitingAndSignalingFromCpuThenTimelineIoctlsAreNotIssued) {
    auto binary0 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    auto binary1 = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);

    const ExternalSemaphoreOperation operations[] = {{binary0.get(), 0u}, {binary1.get(), 0u}};
    EXPECT_TRUE(mockDrm->waitExternalSemaphoresFromCpu(operations));
    EXPECT_TRUE(mockDrm->signalExternalSemaphoresFromCpu(operations));

    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjTimelineWait);
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjTimelineSignal);
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjWait);
    EXPECT_EQ(2u, mockDrm->syncObjWaitCountHandles);
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjSignal);
    EXPECT_EQ(2u, mockDrm->syncObjSignalCountHandles);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineIoctlFailsWhenWaitingOnTimelineAndBinarySemaphoresFromCpuThenFalseIsReturnedAndBinaryWaitIsSkipped) {
    mockDrm->failOnSyncObjTimelineWait = true;

    auto timeline = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    auto binary = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);

    const ExternalSemaphoreOperation waits[] = {{timeline.get(), 1u}, {binary.get(), 0u}};
    EXPECT_FALSE(mockDrm->waitExternalSemaphoresFromCpu(waits));
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjTimelineWait);
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjWait);
}

TEST_F(DrmExternalSemaphoreTest, givenTimelineIoctlFailsWhenSignalingTimelineAndBinarySemaphoresFromCpuThenFalseIsReturnedAndBinarySemaphoresAreStillSignaled) {
    mockDrm->failOnSyncObjTimelineSignal = true;

    auto timeline = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    auto binary = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);

    const ExternalSemaphoreOperation signals[] = {{timeline.get(), 1u}, {binary.get(), 0u}};
    EXPECT_FALSE(mockDrm->signalExternalSemaphoresFromCpu(signals));
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjTimelineSignal);
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjSignal);
}

TEST_F(DrmExternalSemaphoreTest, givenBinaryIoctlFailsWhenSignalingTimelineAndBinarySemaphoresFromCpuThenFalseIsReturnedAndTimelineSemaphoresAreSignaled) {
    mockDrm->failOnSyncObjSignal = true;

    auto timeline = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreFd, nullptr, 0u, nullptr);
    auto binary = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);

    const ExternalSemaphoreOperation signals[] = {{timeline.get(), 1u}, {binary.get(), 0u}};
    EXPECT_FALSE(mockDrm->signalExternalSemaphoresFromCpu(signals));
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjTimelineSignal);
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjSignal);
}

TEST_F(DrmExternalSemaphoreTest, givenNoSemaphoresWhenWaitingAndSignalingFromCpuThenTrueIsReturnedAndNoIoctlIsIssued) {
    EXPECT_TRUE(mockDrm->waitExternalSemaphoresFromCpu({}));
    EXPECT_TRUE(mockDrm->signalExternalSemaphoresFromCpu({}));
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjWait);
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjSignal);
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjTimelineWait);
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjTimelineSignal);
}

} // namespace NEO
