/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/external_semaphore_windows.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

namespace NEO {

using WddmExternalSemaphoreTest = WddmFixture;

TEST_F(WddmExternalSemaphoreTest, givenNullOsInterfaceWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(nullptr, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenOpaqueFdSemaphoreWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenValidD3d12FenceSemaphoreWhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenValidD3d11FenceSemaphoreWhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d11Fence, extSemaphoreHandle, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenValidTimelineSemaphoreWin32WhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, extSemName);
    EXPECT_NE(externalSemaphore, nullptr);
}

class MockWindowsExternalSemaphore : public ExternalSemaphoreWindows {
  public:
    using ExternalSemaphoreWindows::enqueueSignal;
    using ExternalSemaphoreWindows::enqueueWait;

    MockWindowsExternalSemaphore(OSInterface *osInterface, ExternalSemaphore::Type type, uint64_t *signalVal) {
        this->osInterface = osInterface;
        this->type = type;
        this->state = ExternalSemaphore::SemaphoreState::Initial;
        this->syncHandle = 1;
        this->pCpuAddress = nullptr;
        this->pLastSignaledValue = signalVal;
    }

    uint64_t lastSignaledValue() const {
        return pLastSignaledValue ? *pLastSignaledValue : 0u;
    }
};

class MockSyncGdi : public MockGdi {
  public:
    MockSyncGdi() : MockGdi() {
        openSyncObjectNtHandleFromName = mockOpenSyncObjectNtHandleFromName;
        openSyncObjectFromNtHandle2 = mockOpenSyncObjectFromNtHandle2;
        waitForSynchronizationObjectFromCpu = mockWaitForSynchronizationObjectFromCpu;
        signalSynchronizationObjectFromCpu = mockSignalSynchronizationObjectFromCpu;
    }

    static NTSTATUS __stdcall mockOpenSyncObjectNtHandleFromName(IN OUT D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *) {
        return (failOpenSyncObjectNtHandleName ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static NTSTATUS __stdcall mockOpenSyncObjectFromNtHandle2(IN OUT D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *) {
        return (failOpenSyncObjectFromNtHandle ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static NTSTATUS __stdcall mockWaitForSynchronizationObjectFromCpu(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *) {
        return (failWaitForSynchObjectFromCpu ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static NTSTATUS __stdcall mockSignalSynchronizationObjectFromCpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *) {
        return (failSignalSynchObjectFromCpu ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static bool failOpenSyncObjectNtHandleName;
    static bool failOpenSyncObjectFromNtHandle;
    static bool failWaitForSynchObjectFromCpu;
    static bool failSignalSynchObjectFromCpu;
};

bool MockSyncGdi::failOpenSyncObjectNtHandleName = true;
bool MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
bool MockSyncGdi::failWaitForSynchObjectFromCpu = false;
bool MockSyncGdi::failSignalSynchObjectFromCpu = false;

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjFailsWhenEnqueueSignalIsCalledWithOpaqueWin32ThenFalseIsReturnedAndSignalValueIsNotUpdated) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t signalVal = 0u;
    auto extSem = new MockWindowsExternalSemaphore{osInterface, ExternalSemaphore::Type::OpaqueWin32, &signalVal};
    EXPECT_NE(extSem, nullptr);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(extSem->lastSignaledValue(), 0u);
    EXPECT_EQ(signalVal, 0u);

    MockSyncGdi::failSignalSynchObjectFromCpu = true;
    auto result = extSem->enqueueSignal(&signalVal);
    MockSyncGdi::failSignalSynchObjectFromCpu = false;
    EXPECT_EQ(result, false);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(extSem->lastSignaledValue(), signalVal);
    EXPECT_EQ(signalVal, 0ull);

    delete extSem;
}

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjFailsWhenEnqueueWaitIsCalledWithOpaqueWin32ThenFalseIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t signalVal = 0u;
    auto extSem = new MockWindowsExternalSemaphore{osInterface, ExternalSemaphore::Type::OpaqueWin32, &signalVal};
    EXPECT_NE(extSem, nullptr);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(extSem->lastSignaledValue(), 0u);
    EXPECT_EQ(signalVal, 0ull);

    MockSyncGdi::failWaitForSynchObjectFromCpu = true;
    auto result = extSem->enqueueWait(&signalVal);
    MockSyncGdi::failWaitForSynchObjectFromCpu = false;
    EXPECT_EQ(result, false);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(extSem->lastSignaledValue(), signalVal);
    EXPECT_EQ(signalVal, 0ull);

    delete extSem;
}

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjSucceedsWhenEnqueueSignalIsCalledWithOpaqueWin32ThenTrueIsReturnedAndSignalValueIsUpdated) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t signalVal = 0u;
    auto extSem = new MockWindowsExternalSemaphore{osInterface, ExternalSemaphore::Type::OpaqueWin32, &signalVal};
    EXPECT_NE(extSem, nullptr);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(extSem->lastSignaledValue(), 0u);
    EXPECT_EQ(signalVal, 0ull);

    auto result = extSem->enqueueSignal(&signalVal);
    EXPECT_EQ(result, true);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(extSem->lastSignaledValue(), signalVal);
    EXPECT_EQ(signalVal, 2ull);

    delete extSem;
}

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjSucceedsWhenEnqueueWaitIsCalledWithOpaqueWin32ThenTrueIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t signalVal = 0u;
    auto extSem = new MockWindowsExternalSemaphore{osInterface, ExternalSemaphore::Type::OpaqueWin32, &signalVal};
    EXPECT_NE(extSem, nullptr);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(extSem->lastSignaledValue(), 0u);
    EXPECT_EQ(signalVal, 0ull);

    auto result = extSem->enqueueWait(&signalVal);
    EXPECT_EQ(result, true);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(extSem->lastSignaledValue(), signalVal);
    EXPECT_EQ(signalVal, 0ull);

    delete extSem;
}

TEST_F(WddmExternalSemaphoreTest, givenTimelineSemaphoreWin32FailsToOpenSyncObjectFromNameThenNullptrIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, extSemName);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenTimelineSemaphoreWin32FailsToOpenSyncObjectFromNtHandleThenNullptrIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

} // namespace NEO
