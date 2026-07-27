/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/external_semaphore_windows.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <memory>

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
    using ExternalSemaphoreWindows::getNamedObjectDirectoryPath;

    MockWindowsExternalSemaphore(OSInterface *osInterface, ExternalSemaphore::Type type, uint64_t *signalVal) {
        this->osInterface = osInterface;
        this->type = type;
        this->state = ExternalSemaphore::SemaphoreState::Initial;
        this->syncHandle = 1;
        this->pCpuAddress = nullptr;
        this->pLastSignaledValue = signalVal;
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

    static NTSTATUS __stdcall mockOpenSyncObjectNtHandleFromName(IN OUT D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *openSyncObject) {
        openSyncObjectNtHandleFromNameCallCount++;
        if (failOpenSyncObjectNtHandleName) {
            return STATUS_UNSUCCESSFUL;
        }
        openSyncObject->hNtHandle = reinterpret_cast<HANDLE>(0x1);
        return STATUS_SUCCESS;
    }

    static NTSTATUS __stdcall mockOpenSyncObjectFromNtHandle2(IN OUT D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *) {
        return (failOpenSyncObjectFromNtHandle ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static NTSTATUS __stdcall mockWaitForSynchronizationObjectFromCpu(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *) {
        return (failWaitForSynchObjectFromCpu ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static NTSTATUS __stdcall mockSignalSynchronizationObjectFromCpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *signalSyncObject) {
        allowFenceRewindPassedToSignal = signalSyncObject->Flags.AllowFenceRewind;
        return (failSignalSynchObjectFromCpu ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static bool failOpenSyncObjectNtHandleName;
    static bool failOpenSyncObjectFromNtHandle;
    static bool failWaitForSynchObjectFromCpu;
    static bool failSignalSynchObjectFromCpu;
    static uint32_t openSyncObjectNtHandleFromNameCallCount;
    static uint32_t allowFenceRewindPassedToSignal;
};

bool MockSyncGdi::failOpenSyncObjectNtHandleName = true;
bool MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
bool MockSyncGdi::failWaitForSynchObjectFromCpu = false;
bool MockSyncGdi::failSignalSynchObjectFromCpu = false;
uint32_t MockSyncGdi::openSyncObjectNtHandleFromNameCallCount = 0;
uint32_t MockSyncGdi::allowFenceRewindPassedToSignal = 0;

TEST_F(WddmExternalSemaphoreTest, givenOpaqueWin32OrTimelineSemaphoreWin32WhenEnqueueSignalIsCalledThenAllowFenceRewindIsSet) {
    const ExternalSemaphore::Type types[] = {ExternalSemaphore::Type::OpaqueWin32,
                                             ExternalSemaphore::Type::TimelineSemaphoreWin32};

    for (auto type : types) {
        auto mockGdi = new MockSyncGdi();
        static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
        MockSyncGdi::allowFenceRewindPassedToSignal = 0;

        uint64_t lastSignaledValue = 5u;
        auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, type, &lastSignaledValue);

        uint64_t fenceValue = 7u;
        EXPECT_TRUE(extSem->enqueueSignal(&fenceValue));
        EXPECT_EQ(1u, MockSyncGdi::allowFenceRewindPassedToSignal);
    }
}

TEST_F(WddmExternalSemaphoreTest, givenFenceSemaphoreWhenEnqueueSignalIsCalledThenAllowFenceRewindIsNotSet) {
    const ExternalSemaphore::Type types[] = {ExternalSemaphore::Type::D3d12Fence,
                                             ExternalSemaphore::Type::D3d11Fence};

    for (auto type : types) {
        auto mockGdi = new MockSyncGdi();
        static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
        MockSyncGdi::allowFenceRewindPassedToSignal = 1;

        uint64_t lastSignaledValue = 5u;
        auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, type, &lastSignaledValue);

        uint64_t fenceValue = 7u;
        EXPECT_TRUE(extSem->enqueueSignal(&fenceValue));
        EXPECT_EQ(0u, MockSyncGdi::allowFenceRewindPassedToSignal);
    }
}

TEST_F(WddmExternalSemaphoreTest, givenGdiSignalSyncObjFailsWhenEnqueueSignalIsCalledWithOpaqueWin32ThenFalseIsReturnedAndStateIsNotChanged) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);

    uint64_t fenceValue = 7u;

    MockSyncGdi::failSignalSynchObjectFromCpu = true;
    auto result = extSem->enqueueSignal(&fenceValue);
    MockSyncGdi::failSignalSynchObjectFromCpu = false;

    EXPECT_EQ(result, false);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
}

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjFailsWhenEnqueueWaitIsCalledWithOpaqueWin32ThenFalseIsReturnedAndStateIsNotChanged) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);

    uint64_t fenceValue = 7u;

    MockSyncGdi::failWaitForSynchObjectFromCpu = true;
    auto result = extSem->enqueueWait(&fenceValue);
    MockSyncGdi::failWaitForSynchObjectFromCpu = false;

    EXPECT_EQ(result, false);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);
}

TEST_F(WddmExternalSemaphoreTest, givenGdiSignalSyncObjSucceedsWhenEnqueueSignalIsCalledWithOpaqueWin32ThenTrueIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);

    uint64_t fenceValue = 7u;
    auto result = extSem->enqueueSignal(&fenceValue);

    EXPECT_EQ(result, true);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Signaled);
}

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjSucceedsWhenEnqueueWaitIsCalledWithOpaqueWin32ThenTrueIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);

    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Initial);

    uint64_t fenceValue = 7u;
    auto result = extSem->enqueueWait(&fenceValue);

    EXPECT_EQ(result, true);
    EXPECT_EQ(extSem->getState(), ExternalSemaphore::SemaphoreState::Signaled);
}

TEST_F(WddmExternalSemaphoreTest, givenOpaqueWin32SemaphoreWhenAcquireSignalFenceValueIsCalledThenLastSignaledValueIsIncrementedByTwoAndReturned) {
    uint64_t lastSignaledValue = 10u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);

    EXPECT_EQ(extSem->acquireSignalFenceValue(0u), 12ull);
    EXPECT_EQ(lastSignaledValue, 12ull);

    EXPECT_EQ(extSem->acquireSignalFenceValue(1000u), 14ull);
    EXPECT_EQ(lastSignaledValue, 14ull);
}

TEST_F(WddmExternalSemaphoreTest, givenOpaqueWin32SemaphoreWhenAcquireWaitFenceValueIsCalledThenLastSignaledValueIsReturnedAndNotModified) {
    uint64_t lastSignaledValue = 10u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);

    EXPECT_EQ(extSem->acquireWaitFenceValue(0u), 10ull);
    EXPECT_EQ(extSem->acquireWaitFenceValue(1000u), 10ull);
    EXPECT_EQ(lastSignaledValue, 10ull);

    lastSignaledValue = 20u;
    EXPECT_EQ(extSem->acquireWaitFenceValue(1000u), 20ull);
    EXPECT_EQ(lastSignaledValue, 20ull);
}

TEST_F(WddmExternalSemaphoreTest, givenNonOpaqueWin32SemaphoreWhenAcquireFenceValueIsCalledThenPassedValueIsReturnedAndLastSignaledValueIsNotModified) {
    const ExternalSemaphore::Type types[] = {ExternalSemaphore::Type::TimelineSemaphoreWin32,
                                             ExternalSemaphore::Type::D3d12Fence,
                                             ExternalSemaphore::Type::D3d11Fence};

    for (auto type : types) {
        uint64_t lastSignaledValue = 10u;
        auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, type, &lastSignaledValue);

        EXPECT_EQ(extSem->acquireWaitFenceValue(123u), 123ull);
        EXPECT_EQ(extSem->acquireSignalFenceValue(321u), 321ull);
        EXPECT_EQ(lastSignaledValue, 10ull);
    }
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

TEST_F(WddmExternalSemaphoreTest, givenD3d12FenceWithNameWhenOpenSyncObjectFromNameFailsThenNullptrIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "d3d12_fence_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, extSemName);
    EXPECT_EQ(externalSemaphore, nullptr);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenD3d11FenceWithNameWhenOpenSyncObjectFromNameFailsThenNullptrIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "d3d11_fence_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d11Fence, extSemaphoreHandle, 0u, extSemName);
    EXPECT_EQ(externalSemaphore, nullptr);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenD3d12FenceWithNameWhenOpenSyncObjectFromNameSucceedsThenSemaphoreIsSuccessfullyReturnedAndNameLookupIsInvoked) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    MockSyncGdi::openSyncObjectNtHandleFromNameCallCount = 0;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "d3d12_fence_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, extSemName);
    EXPECT_NE(externalSemaphore, nullptr);
    EXPECT_EQ(MockSyncGdi::openSyncObjectNtHandleFromNameCallCount, 1u);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenD3d11FenceWithNameWhenOpenSyncObjectFromNameSucceedsThenSemaphoreIsSuccessfullyReturnedAndNameLookupIsInvoked) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    MockSyncGdi::openSyncObjectNtHandleFromNameCallCount = 0;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "d3d11_fence_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d11Fence, extSemaphoreHandle, 0u, extSemName);
    EXPECT_NE(externalSemaphore, nullptr);
    EXPECT_EQ(MockSyncGdi::openSyncObjectNtHandleFromNameCallCount, 1u);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST(WddmExternalSemaphoreNamespaceTest, givenGlobalPrefixedNameWhenResolvingDirectoryThenGlobalBaseNamedObjectsIsUsedAndPrefixIsStripped) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = MockWindowsExternalSemaphore::getNamedObjectDirectoryPath(4u, L"Global\\myFence", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myFence");
}

TEST(WddmExternalSemaphoreNamespaceTest, givenGlobalPrefixedNameInSessionZeroWhenResolvingDirectoryThenGlobalBaseNamedObjectsIsUsed) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = MockWindowsExternalSemaphore::getNamedObjectDirectoryPath(0u, L"Global\\myFence", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myFence");
}

TEST(WddmExternalSemaphoreNamespaceTest, givenLocalPrefixedNameWithNonZeroSessionWhenResolvingDirectoryThenSessionDirectoryIsUsedAndPrefixIsStripped) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = MockWindowsExternalSemaphore::getNamedObjectDirectoryPath(3u, L"Local\\myFence", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\Sessions\\3\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myFence");
}

TEST(WddmExternalSemaphoreNamespaceTest, givenLocalPrefixedNameInSessionZeroWhenResolvingDirectoryThenBaseNamedObjectsIsUsedAndPrefixIsStripped) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = MockWindowsExternalSemaphore::getNamedObjectDirectoryPath(0u, L"Local\\myFence", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myFence");
}

TEST(WddmExternalSemaphoreNamespaceTest, givenUnprefixedNameWithNonZeroSessionWhenResolvingDirectoryThenSessionDirectoryIsUsedAndNameIsUnchanged) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = MockWindowsExternalSemaphore::getNamedObjectDirectoryPath(7u, L"myFence", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\Sessions\\7\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myFence");
}

TEST(WddmExternalSemaphoreNamespaceTest, givenUnprefixedNameInSessionZeroWhenResolvingDirectoryThenBaseNamedObjectsIsUsedAndNameIsUnchanged) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = MockWindowsExternalSemaphore::getNamedObjectDirectoryPath(0u, L"myFence", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myFence");
}

TEST_F(WddmExternalSemaphoreTest, givenNamedSemaphoreWithNonZeroSessionWhenCreatingThenSessionDirectoryIsOpenedAndSemaphoreIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;

    VariableBackup<decltype(SysCalls::sysCallsProcessIdToSessionId)> backupSessionId(&SysCalls::sysCallsProcessIdToSessionId, [](DWORD, DWORD *pSessionId) -> BOOL {
        *pSessionId = 5;
        return TRUE;
    });
    VariableBackup<decltype(SysCalls::sysCallsNtOpenDirectoryObject)> backupOpenDir(&SysCalls::sysCallsNtOpenDirectoryObject, [](PHANDLE directoryHandle, ACCESS_MASK, POBJECT_ATTRIBUTES objectAttributes) -> NTSTATUS {
        EXPECT_STREQ(L"\\Sessions\\5\\BaseNamedObjects", objectAttributes->ObjectName->Buffer);
        *directoryHandle = reinterpret_cast<HANDLE>(0x7);
        return STATUS_SUCCESS;
    });

    HANDLE extSemaphoreHandle = 0;
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, "myFence");
    EXPECT_NE(externalSemaphore, nullptr);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenNamedSemaphoreInSessionZeroWhenCreatingThenBaseNamedObjectsDirectoryIsOpened) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;

    VariableBackup<decltype(SysCalls::sysCallsProcessIdToSessionId)> backupSessionId(&SysCalls::sysCallsProcessIdToSessionId, [](DWORD, DWORD *pSessionId) -> BOOL {
        *pSessionId = 0;
        return TRUE;
    });
    VariableBackup<decltype(SysCalls::sysCallsNtOpenDirectoryObject)> backupOpenDir(&SysCalls::sysCallsNtOpenDirectoryObject, [](PHANDLE directoryHandle, ACCESS_MASK, POBJECT_ATTRIBUTES objectAttributes) -> NTSTATUS {
        EXPECT_STREQ(L"\\BaseNamedObjects", objectAttributes->ObjectName->Buffer);
        *directoryHandle = reinterpret_cast<HANDLE>(0x7);
        return STATUS_SUCCESS;
    });

    HANDLE extSemaphoreHandle = 0;
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, "myFence");
    EXPECT_NE(externalSemaphore, nullptr);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenNamedSemaphoreWhenNtOpenDirectoryObjectFailsThenNullptrIsReturned) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;

    VariableBackup<decltype(SysCalls::sysCallsNtOpenDirectoryObject)> backupOpenDir(&SysCalls::sysCallsNtOpenDirectoryObject, [](PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES) -> NTSTATUS {
        return STATUS_UNSUCCESSFUL;
    });

    HANDLE extSemaphoreHandle = 0;
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, "myFence");
    EXPECT_EQ(externalSemaphore, nullptr);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenNamedSemaphoreWhenCreatedSuccessfullyThenRootDirectoryAndSyncHandleAreClosed) {
    auto mockGdi = new MockSyncGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;

    auto closeHandleCallsBefore = SysCalls::closeHandleCalled;

    HANDLE extSemaphoreHandle = 0;
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, "myFence");
    EXPECT_NE(externalSemaphore, nullptr);
    EXPECT_EQ(closeHandleCallsBefore + 2u, SysCalls::closeHandleCalled);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

} // namespace NEO
