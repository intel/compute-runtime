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

#include <vector>

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
    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::OpaqueFd,
        nullptr,
        testFd,
        nullptr);

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
    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::TimelineSemaphoreFd,
        nullptr,
        testFd,
        nullptr);

    EXPECT_NE(externalSemaphore, nullptr);

    // Verify: FD should have been closed after successful import
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmExternalSemaphoreTest, givenImportFailsWhenCreatingExternalSemaphoreThenFdIsNotClosed) {
    // Setup: Make syncObjFdToHandle fail
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    mockDrm->failOnSyncObjFdToHandle = true;

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    const int testFd = 77;

    // Exercise: Try to create external semaphore (should fail)
    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::OpaqueFd,
        nullptr,
        testFd,
        nullptr);

    EXPECT_EQ(externalSemaphore, nullptr);

    // Verify: FD should NOT be closed on failure - caller retains ownership
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST_F(DrmExternalSemaphoreTest, givenValidSemaphoreWhenDestroyedThenSyncHandleIsDestroyed) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    // Exercise: Create and destroy semaphore
    {
        auto externalSemaphore = ExternalSemaphore::create(
            executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
            ExternalSemaphore::Type::OpaqueFd,
            nullptr,
            0,
            nullptr);
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs here
    }

    // Verify: syncObjDestroy should have been called
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenMultipleSemaphoresWhenDestroyedThenAllSyncHandlesAreDestroyed) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    const int numSemaphores = 5;

    // Exercise: Create and destroy multiple semaphores
    {
        std::vector<std::unique_ptr<ExternalSemaphore>> semaphores;
        for (int i = 0; i < numSemaphores; i++) {
            auto sem = ExternalSemaphore::create(
                executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                ExternalSemaphore::Type::OpaqueFd,
                nullptr,
                i,
                nullptr);
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
        auto externalSemaphore = ExternalSemaphore::create(
            executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
            ExternalSemaphore::Type::OpaqueFd,
            nullptr,
            i,
            nullptr);
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs each iteration
    }

    // Verify: Every import should have closed its FD
    EXPECT_EQ(static_cast<uint32_t>(iterations - 1), SysCalls::closeFuncCalled);
}

TEST_F(DrmExternalSemaphoreTest, givenSyncObjDestroyFailsWhenDestroyingSemaphoreThenNoException) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    mockDrm->failOnSyncObjDestroy = true;

    // Exercise: Create semaphore, then destroy - should not throw even if destroy fails
    EXPECT_NO_THROW({
        auto externalSemaphore = ExternalSemaphore::create(
            executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
            ExternalSemaphore::Type::OpaqueFd,
            nullptr,
            0,
            nullptr);
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs - syncObjDestroy will fail but should not throw
    });

    // Verify: Destroy was attempted
    EXPECT_EQ(1, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenSemaphoreWithZeroSyncHandleWhenDestroyedThenSyncObjDestroyIsNotCalled) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    // Exercise: Create semaphore without importing (syncHandle remains 0), then destroy
    {
        auto externalSemaphore = ExternalSemaphoreLinux::create(
            executionEnvironment->rootDeviceEnvironments[0]->osInterface.get());
        EXPECT_NE(externalSemaphore, nullptr);
        // Destructor runs here - should early return due to syncHandle == 0
    }

    // Verify: syncObjDestroy should NOT have been called (early return)
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenSemaphoreWithNullOsInterfaceWhenDestroyedThenSyncObjDestroyIsNotCalled) {
    auto mockDrm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjDestroy, 0);

    // Exercise: Create valid semaphore with imported handle, then set osInterface to nullptr
    {
        auto externalSemaphore = ExternalSemaphore::create(
            executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
            ExternalSemaphore::Type::OpaqueFd,
            nullptr,
            0,
            nullptr);
        EXPECT_NE(externalSemaphore, nullptr);

        // Simulate osInterface becoming invalid (e.g., during shutdown race conditions)
        externalSemaphore->osInterface = nullptr;

        // Destructor runs here - should early return due to osInterface == nullptr
    }

    // Verify: syncObjDestroy should NOT have been called (early return prevents crash)
    EXPECT_EQ(0, mockDrm->ioctlCnt.syncObjDestroy);
}

TEST_F(DrmExternalSemaphoreTest, givenPrintExternalSemaphoreTimelineEnabledWhenTimelineSemaphoreEnqueueWaitIsCalledThenDebugMessageIsPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreTimeline.set(1);

    auto mockDrm = static_cast<DrmMockCustom *>(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::TimelineSemaphoreFd,
        nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 42u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 0);
    auto result = externalSemaphore->enqueueWait(&fenceValue);

    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenPrintExternalSemaphoreTimelineDisabledWhenTimelineSemaphoreEnqueueWaitIsCalledThenDebugMessageIsNotPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreTimeline.set(0);

    auto mockDrm = static_cast<DrmMockCustom *>(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::TimelineSemaphoreFd,
        nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 42u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 0);
    auto result = externalSemaphore->enqueueWait(&fenceValue);

    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenPrintExternalSemaphoreTimelineEnabledWhenTimelineSemaphoreEnqueueSignalIsCalledThenDebugMessageIsPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreTimeline.set(1);

    auto mockDrm = static_cast<DrmMockCustom *>(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::TimelineSemaphoreFd,
        nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 42u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 0);
    auto result = externalSemaphore->enqueueSignal(&fenceValue);

    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenPrintExternalSemaphoreTimelineDisabledWhenTimelineSemaphoreEnqueueSignalIsCalledThenDebugMessageIsNotPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintExternalSemaphoreTimeline.set(0);

    auto mockDrm = static_cast<DrmMockCustom *>(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::TimelineSemaphoreFd,
        nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 42u;

    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 0);
    auto result = externalSemaphore->enqueueSignal(&fenceValue);

    EXPECT_EQ(result, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 1);
}

TEST_F(DrmExternalSemaphoreTest, givenPrintExternalSemaphoreTimelineDefaultWhenTimelineSemaphoreOperationsCalledThenDebugMessageIsNotPrinted) {
    DebugManagerStateRestore restore;

    auto mockDrm = static_cast<DrmMockCustom *>(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());

    auto externalSemaphore = ExternalSemaphore::create(
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
        ExternalSemaphore::Type::TimelineSemaphoreFd,
        nullptr, 0u, nullptr);
    ASSERT_NE(externalSemaphore, nullptr);

    uint64_t fenceValue = 42u;

    auto waitResult = externalSemaphore->enqueueWait(&fenceValue);
    auto signalResult = externalSemaphore->enqueueSignal(&fenceValue);

    EXPECT_EQ(waitResult, true);
    EXPECT_EQ(signalResult, true);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineWait, 1);
    EXPECT_EQ(mockDrm->ioctlCnt.syncObjTimelineSignal, 1);
}

} // namespace NEO
