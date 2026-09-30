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
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace NEO {
class MockWindowsExternalSemaphore : public ExternalSemaphoreWindows {
  public:
    using ExternalSemaphoreWindows::convertUtf8NameToWide;
    using ExternalSemaphoreWindows::getNamedObjectDirectoryPath;
    using ExternalSemaphoreWindows::maxNameLengthInWideChars;
    using ExternalSemaphoreWindows::syncHandle;

    MockWindowsExternalSemaphore(OSInterface *osInterface, ExternalSemaphore::Type type, uint64_t *signalVal) {
        this->osInterface = osInterface;
        this->type = type;
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
        if ((lastObjectName != nullptr) && (openSyncObject->pObjAttrib != nullptr) &&
            (openSyncObject->pObjAttrib->ObjectName != nullptr) && (openSyncObject->pObjAttrib->ObjectName->Buffer != nullptr)) {
            lastObjectName->assign(openSyncObject->pObjAttrib->ObjectName->Buffer);
        }
        if (failOpenSyncObjectNtHandleName) {
            return STATUS_UNSUCCESSFUL;
        }
        openSyncObject->hNtHandle = reinterpret_cast<HANDLE>(0x1);
        return STATUS_SUCCESS;
    }

    static NTSTATUS __stdcall mockOpenSyncObjectFromNtHandle2(IN OUT D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *) {
        return (failOpenSyncObjectFromNtHandle ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static NTSTATUS __stdcall mockWaitForSynchronizationObjectFromCpu(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *waitSyncObject) {
        waitFromCpuCallCount++;
        captureObjects(waitSyncObject->ObjectCount, waitSyncObject->ObjectHandleArray, waitSyncObject->FenceValueArray);
        return (failWaitForSynchObjectFromCpu ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static NTSTATUS __stdcall mockSignalSynchronizationObjectFromCpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *signalSyncObject) {
        signalFromCpuCallCount++;
        allowFenceRewindPassedToSignal = signalSyncObject->Flags.AllowFenceRewind;
        captureObjects(signalSyncObject->ObjectCount, signalSyncObject->ObjectHandleArray, signalSyncObject->FenceValueArray);
        return (failSignalSynchObjectFromCpu ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS);
    }

    static bool failOpenSyncObjectNtHandleName;
    static bool failOpenSyncObjectFromNtHandle;
    static bool failWaitForSynchObjectFromCpu;
    static bool failSignalSynchObjectFromCpu;
    static uint32_t openSyncObjectNtHandleFromNameCallCount;
    static uint32_t allowFenceRewindPassedToSignal;
    static uint32_t waitFromCpuCallCount;
    static uint32_t signalFromCpuCallCount;

    struct CapturedObjects {
        static constexpr uint32_t maxCalls = 4u;
        static constexpr uint32_t maxObjectsPerCall = D3DDDI_MAX_OBJECT_WAITED_ON;
        struct Call {
            uint32_t count = 0u;
            std::array<D3DKMT_HANDLE, maxObjectsPerCall> handles = {};
            std::array<uint64_t, maxObjectsPerCall> fenceValues = {};
        };
        uint32_t calls = 0u;
        std::array<Call, maxCalls> perCall = {};
    };
    static CapturedObjects capturedObjects;

    static void captureObjects(uint32_t count, const D3DKMT_HANDLE *handles, const uint64_t *values) {
        if (capturedObjects.calls < CapturedObjects::maxCalls) {
            auto &call = capturedObjects.perCall[capturedObjects.calls];
            call.count = count;
            for (uint32_t i = 0; i < count && i < CapturedObjects::maxObjectsPerCall; i++) {
                call.handles[i] = handles[i];
                call.fenceValues[i] = values[i];
            }
        }
        capturedObjects.calls++;
    }
    // Points at a test-local string; the mock must not own heap memory that outlives a test.
    static std::wstring *lastObjectName;
};

bool MockSyncGdi::failOpenSyncObjectNtHandleName = true;
bool MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
bool MockSyncGdi::failWaitForSynchObjectFromCpu = false;
bool MockSyncGdi::failSignalSynchObjectFromCpu = false;
uint32_t MockSyncGdi::openSyncObjectNtHandleFromNameCallCount = 0;
uint32_t MockSyncGdi::allowFenceRewindPassedToSignal = 0;
uint32_t MockSyncGdi::waitFromCpuCallCount = 0;
uint32_t MockSyncGdi::signalFromCpuCallCount = 0;
MockSyncGdi::CapturedObjects MockSyncGdi::capturedObjects = {};
std::wstring *MockSyncGdi::lastObjectName = nullptr;

struct WddmExternalSemaphoreTest : public WddmFixture {
    void SetUp() override {
        WddmFixture::SetUp();
        static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi = std::make_unique<MockSyncGdi>();
    }

    static std::array<ExternalSemaphoreOperation, 1> singleOperation(const ExternalSemaphore &semaphore, uint64_t fenceValue) {
        return {{{&semaphore, fenceValue}}};
    }
};

TEST_F(WddmExternalSemaphoreTest, givenNullOsInterfaceWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    HANDLE extSemaphoreHandle = 0;
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(nullptr, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, nullptr, importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::unsupported, importResult);
}

TEST_F(WddmExternalSemaphoreTest, givenOpaqueFdSemaphoreWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr, importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::unsupported, importResult);
}

TEST_F(WddmExternalSemaphoreTest, givenValidD3d12FenceSemaphoreWhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    VariableBackup<bool> openFromNtHandleBackup(&MockSyncGdi::failOpenSyncObjectFromNtHandle, false);
    HANDLE extSemaphoreHandle = 0;
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::unsupported;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, nullptr, importResult);
    EXPECT_NE(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::success, importResult);
}

TEST_F(WddmExternalSemaphoreTest, givenValidD3d11FenceSemaphoreWhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    VariableBackup<bool> openFromNtHandleBackup(&MockSyncGdi::failOpenSyncObjectFromNtHandle, false);
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d11Fence, extSemaphoreHandle, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenValidTimelineSemaphoreWin32WhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    VariableBackup<bool> openNtHandleFromNameBackup(&MockSyncGdi::failOpenSyncObjectNtHandleName, false);
    VariableBackup<bool> openFromNtHandleBackup(&MockSyncGdi::failOpenSyncObjectFromNtHandle, false);
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, extSemName);
    EXPECT_NE(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenOpaqueWin32OrTimelineSemaphoreWin32WhenSignalExternalSemaphoresFromCpuIsCalledThenAllowFenceRewindIsSet) {
    const ExternalSemaphore::Type types[] = {ExternalSemaphore::Type::OpaqueWin32,
                                             ExternalSemaphore::Type::TimelineSemaphoreWin32};

    for (auto type : types) {
        MockSyncGdi::allowFenceRewindPassedToSignal = 0;

        uint64_t lastSignaledValue = 5u;
        auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, type, &lastSignaledValue);

        uint64_t fenceValue = 7u;
        EXPECT_TRUE(wddm->signalExternalSemaphoresFromCpu(singleOperation(*extSem, fenceValue)));
        EXPECT_EQ(1u, MockSyncGdi::allowFenceRewindPassedToSignal);
    }
}

TEST_F(WddmExternalSemaphoreTest, givenFenceSemaphoreWhenSignalExternalSemaphoresFromCpuIsCalledThenSuccessIsReturned) {
    const ExternalSemaphore::Type types[] = {ExternalSemaphore::Type::D3d12Fence,
                                             ExternalSemaphore::Type::D3d11Fence};

    for (auto type : types) {
        uint64_t lastSignaledValue = 5u;
        auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, type, &lastSignaledValue);

        uint64_t fenceValue = 7u;
        EXPECT_TRUE(wddm->signalExternalSemaphoresFromCpu(singleOperation(*extSem, fenceValue)));
    }
}

TEST_F(WddmExternalSemaphoreTest, givenGdiSignalSyncObjFailsWhenSignalExternalSemaphoresFromCpuIsCalledWithOpaqueWin32ThenFalseIsReturned) {
    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);

    uint64_t fenceValue = 7u;

    MockSyncGdi::failSignalSynchObjectFromCpu = true;
    auto result = wddm->signalExternalSemaphoresFromCpu(singleOperation(*extSem, fenceValue));
    MockSyncGdi::failSignalSynchObjectFromCpu = false;

    EXPECT_EQ(result, false);
}

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjFailsWhenWaitExternalSemaphoresFromCpuIsCalledWithOpaqueWin32ThenFalseIsReturned) {
    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);

    uint64_t fenceValue = 7u;

    MockSyncGdi::failWaitForSynchObjectFromCpu = true;
    auto result = wddm->waitExternalSemaphoresFromCpu(singleOperation(*extSem, fenceValue));
    MockSyncGdi::failWaitForSynchObjectFromCpu = false;

    EXPECT_EQ(result, false);
}

TEST_F(WddmExternalSemaphoreTest, givenGdiSignalSyncObjSucceedsWhenSignalExternalSemaphoresFromCpuIsCalledWithOpaqueWin32ThenTrueIsReturned) {
    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);

    uint64_t fenceValue = 7u;
    auto result = wddm->signalExternalSemaphoresFromCpu(singleOperation(*extSem, fenceValue));

    EXPECT_EQ(result, true);
}

TEST_F(WddmExternalSemaphoreTest, givenGdiWaitForSyncObjSucceedsWhenWaitExternalSemaphoresFromCpuIsCalledWithOpaqueWin32ThenTrueIsReturned) {
    uint64_t lastSignaledValue = 5u;
    auto extSem = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::OpaqueWin32, &lastSignaledValue);

    uint64_t fenceValue = 7u;
    auto result = wddm->waitExternalSemaphoresFromCpu(singleOperation(*extSem, fenceValue));

    EXPECT_EQ(result, true);
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
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, extSemName);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenTimelineSemaphoreWin32FailsToOpenSyncObjectFromNtHandleThenNullptrIsReturned) {
    HANDLE extSemaphoreHandle = 0;
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, nullptr, importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::invalidResource, importResult);
}

TEST_F(WddmExternalSemaphoreTest, givenD3d12FenceWithNameWhenOpenSyncObjectFromNameFailsThenNullptrIsReturned) {
    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "d3d12_fence_name";
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, extSemName, importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::invalidResource, importResult);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenD3d11FenceWithNameWhenOpenSyncObjectFromNameFailsThenNullptrIsReturned) {
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
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    MockSyncGdi::openSyncObjectNtHandleFromNameCallCount = 0;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "d3d12_fence_name";
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::unsupported;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, extSemName, importResult);
    EXPECT_NE(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::success, importResult);
    EXPECT_EQ(MockSyncGdi::openSyncObjectNtHandleFromNameCallCount, 1u);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenD3d11FenceWithNameWhenOpenSyncObjectFromNameSucceedsThenSemaphoreIsSuccessfullyReturnedAndNameLookupIsInvoked) {
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

TEST(WddmExternalSemaphoreUtf8Test, givenValidUtf8NameWhenConvertingToWideThenCodePointsAreDecoded) {
    struct NameCase {
        const char *utf8Name;
        const wchar_t *expectedWideName;
        size_t expectedLength;
    };

    const NameCase nameCases[] = {
        {"myFence", L"myFence", 7u},
        {"caf\xC3\xA9", L"caf\u00e9", 4u},
        {"\xE4\xB8\xAD", L"\u4e2d", 1u},
        {"\xF0\x9F\x98\x80", L"\U0001f600", 2u},
    };

    for (const auto &nameCase : nameCases) {
        auto wideName = MockWindowsExternalSemaphore::convertUtf8NameToWide(nameCase.utf8Name);
        EXPECT_STREQ(nameCase.expectedWideName, wideName.c_str());
        EXPECT_EQ(nameCase.expectedLength, wideName.size());
    }
}

TEST(WddmExternalSemaphoreUtf8Test, givenMalformedOrEmptyUtf8NameWhenConvertingToWideThenConversionFails) {
    const char *invalidNames[] = {
        "",
        "\x80",
        "\xC3",
        "\xC0\xAF",
        "\xF8\x88\x80\x80\x80",
        "\xF4\x90\x80\x80",
    };

    for (const auto &invalidName : invalidNames) {
        EXPECT_TRUE(MockWindowsExternalSemaphore::convertUtf8NameToWide(invalidName).empty());
    }
}

TEST(WddmExternalSemaphoreUtf8Test, givenNameExceedingMaxLengthWhenConvertingToWideThenConversionFails) {
    const std::string tooLongName(MockWindowsExternalSemaphore::maxNameLengthInWideChars + 1, 'a');
    EXPECT_TRUE(MockWindowsExternalSemaphore::convertUtf8NameToWide(tooLongName.c_str()).empty());

    const std::string maxLengthName(MockWindowsExternalSemaphore::maxNameLengthInWideChars, 'a');
    EXPECT_EQ(MockWindowsExternalSemaphore::maxNameLengthInWideChars,
              MockWindowsExternalSemaphore::convertUtf8NameToWide(maxLengthName.c_str()).size());
}

TEST_F(WddmExternalSemaphoreTest, givenUtf8NamedSemaphoreWhenCreatingThenDecodedWideNameIsPassedToOs) {
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    MockSyncGdi::openSyncObjectNtHandleFromNameCallCount = 0;
    HANDLE extSemaphoreHandle = 0;

    std::wstring capturedName;
    VariableBackup<decltype(MockSyncGdi::lastObjectName)> backupCapturedName(&MockSyncGdi::lastObjectName, &capturedName);

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, "caf\xC3\xA9");
    EXPECT_NE(externalSemaphore, nullptr);
    EXPECT_EQ(std::wstring(L"caf\u00e9"), capturedName);

    capturedName.clear();
    auto globalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, "Global\\caf\xC3\xA9");
    EXPECT_NE(globalSemaphore, nullptr);
    EXPECT_EQ(std::wstring(L"caf\u00e9"), capturedName);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenInvalidUtf8NamedSemaphoreWhenCreatingThenInvalidResourceIsReturnedAndNameLookupIsNotInvoked) {
    MockSyncGdi::failOpenSyncObjectNtHandleName = false;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = false;
    MockSyncGdi::openSyncObjectNtHandleFromNameCallCount = 0;
    HANDLE extSemaphoreHandle = 0;
    ExternalSemaphore::ImportResult importResult = ExternalSemaphore::ImportResult::success;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, "\xC3", importResult);
    EXPECT_EQ(externalSemaphore, nullptr);
    EXPECT_EQ(ExternalSemaphore::ImportResult::invalidResource, importResult);
    EXPECT_EQ(0u, MockSyncGdi::openSyncObjectNtHandleFromNameCallCount);

    MockSyncGdi::failOpenSyncObjectNtHandleName = true;
    MockSyncGdi::failOpenSyncObjectFromNtHandle = true;
}

TEST_F(WddmExternalSemaphoreTest, givenNamedSemaphoreWithNonZeroSessionWhenCreatingThenSessionDirectoryIsOpenedAndSemaphoreIsReturned) {
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

TEST_F(WddmExternalSemaphoreTest, givenMultipleSemaphoresWhenWaitingFromCpuThenSingleKmdCallIsMadeForAllOfThem) {
    VariableBackup<uint32_t> waitCountBackup(&MockSyncGdi::waitFromCpuCallCount, 0u);
    VariableBackup<MockSyncGdi::CapturedObjects> capturedObjectsBackup(&MockSyncGdi::capturedObjects, {});

    uint64_t lastSignaledValue = 0u;
    auto extSem0 = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::D3d12Fence, &lastSignaledValue);
    auto extSem1 = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, &lastSignaledValue);
    extSem0->syncHandle = 0x10;
    extSem1->syncHandle = 0x20;

    const ExternalSemaphoreOperation waits[] = {{extSem0.get(), 3u}, {extSem1.get(), 4u}};
    EXPECT_TRUE(wddm->waitExternalSemaphoresFromCpu(waits));

    EXPECT_EQ(1u, MockSyncGdi::waitFromCpuCallCount);
    ASSERT_EQ(1u, MockSyncGdi::capturedObjects.calls);
    ASSERT_EQ(2u, MockSyncGdi::capturedObjects.perCall[0].count);
    EXPECT_EQ(0x10u, MockSyncGdi::capturedObjects.perCall[0].handles[0]);
    EXPECT_EQ(0x20u, MockSyncGdi::capturedObjects.perCall[0].handles[1]);
    EXPECT_EQ(3u, MockSyncGdi::capturedObjects.perCall[0].fenceValues[0]);
    EXPECT_EQ(4u, MockSyncGdi::capturedObjects.perCall[0].fenceValues[1]);
}

TEST_F(WddmExternalSemaphoreTest, givenMultipleSemaphoresWhenSignalingFromCpuThenSingleKmdCallIsMadeForAllOfThem) {
    VariableBackup<uint32_t> signalCountBackup(&MockSyncGdi::signalFromCpuCallCount, 0u);
    VariableBackup<MockSyncGdi::CapturedObjects> capturedObjectsBackup(&MockSyncGdi::capturedObjects, {});

    uint64_t lastSignaledValue = 0u;
    auto extSem0 = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::D3d12Fence, &lastSignaledValue);
    auto extSem1 = std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, &lastSignaledValue);
    extSem0->syncHandle = 0x10;
    extSem1->syncHandle = 0x20;

    const ExternalSemaphoreOperation signals[] = {{extSem0.get(), 3u}, {extSem1.get(), 4u}};
    EXPECT_TRUE(wddm->signalExternalSemaphoresFromCpu(signals));

    EXPECT_EQ(1u, MockSyncGdi::signalFromCpuCallCount);
    ASSERT_EQ(1u, MockSyncGdi::capturedObjects.calls);
    ASSERT_EQ(2u, MockSyncGdi::capturedObjects.perCall[0].count);
    EXPECT_EQ(0x10u, MockSyncGdi::capturedObjects.perCall[0].handles[0]);
    EXPECT_EQ(0x20u, MockSyncGdi::capturedObjects.perCall[0].handles[1]);
    EXPECT_EQ(3u, MockSyncGdi::capturedObjects.perCall[0].fenceValues[0]);
    EXPECT_EQ(4u, MockSyncGdi::capturedObjects.perCall[0].fenceValues[1]);
}

TEST_F(WddmExternalSemaphoreTest, givenNoSemaphoresWhenWaitingAndSignalingFromCpuThenTrueIsReturnedAndKmdIsNotCalled) {
    VariableBackup<uint32_t> waitCountBackup(&MockSyncGdi::waitFromCpuCallCount, 0u);
    VariableBackup<uint32_t> signalCountBackup(&MockSyncGdi::signalFromCpuCallCount, 0u);

    EXPECT_TRUE(wddm->waitExternalSemaphoresFromCpu({}));
    EXPECT_TRUE(wddm->signalExternalSemaphoresFromCpu({}));
    EXPECT_EQ(0u, MockSyncGdi::waitFromCpuCallCount);
    EXPECT_EQ(0u, MockSyncGdi::signalFromCpuCallCount);
}

struct WddmExternalSemaphoreBatchTest : public WddmExternalSemaphoreTest {
    void SetUp() override {
        WddmExternalSemaphoreTest::SetUp();
        for (uint32_t i = 0; i < numSemaphores; ++i) {
            auto &semaphore = semaphores.emplace_back(std::make_unique<MockWindowsExternalSemaphore>(osInterface, ExternalSemaphore::Type::D3d12Fence, &lastSignaledValue));
            semaphore->syncHandle = i;
            operations.push_back({semaphore.get(), i});
        }
    }

    static constexpr uint32_t maxPerCall = MockSyncGdi::CapturedObjects::maxObjectsPerCall;
    static constexpr uint32_t numSemaphores = 2 * maxPerCall + 1; // full KMD calls and a partial one
    static constexpr uint32_t expectedCalls = (numSemaphores + maxPerCall - 1) / maxPerCall;
    static_assert(expectedCalls <= MockSyncGdi::CapturedObjects::maxCalls);
    uint64_t lastSignaledValue = 0u;
    std::vector<std::unique_ptr<MockWindowsExternalSemaphore>> semaphores;
    std::vector<ExternalSemaphoreOperation> operations;

    // Calls are filled up to the limit and together pass all semaphores, in order
    void expectBatchSplitIntoCallsWithinLimit() const {
        ASSERT_EQ(expectedCalls, MockSyncGdi::capturedObjects.calls);
        uint32_t semaphoreIndex = 0;
        for (uint32_t call = 0; call < expectedCalls; call++) {
            const auto &captured = MockSyncGdi::capturedObjects.perCall[call];
            const uint32_t remaining = numSemaphores - semaphoreIndex;
            EXPECT_EQ(remaining < maxPerCall ? remaining : maxPerCall, captured.count) << call;
            for (uint32_t i = 0; i < captured.count; i++, semaphoreIndex++) {
                EXPECT_EQ(semaphoreIndex, captured.handles[i]);
                EXPECT_EQ(semaphoreIndex, captured.fenceValues[i]);
            }
        }
        EXPECT_EQ(numSemaphores, semaphoreIndex);
    }
};

TEST_F(WddmExternalSemaphoreBatchTest, givenMoreSemaphoresThanKmdLimitWhenWaitingFromCpuThenWaitIsSplitIntoCallsWithinLimit) {
    VariableBackup<uint32_t> waitCountBackup(&MockSyncGdi::waitFromCpuCallCount, 0u);
    VariableBackup<MockSyncGdi::CapturedObjects> capturedObjectsBackup(&MockSyncGdi::capturedObjects, {});

    EXPECT_TRUE(wddm->waitExternalSemaphoresFromCpu(operations));

    EXPECT_EQ(expectedCalls, MockSyncGdi::waitFromCpuCallCount);
    expectBatchSplitIntoCallsWithinLimit();
}

TEST_F(WddmExternalSemaphoreBatchTest, givenMoreSemaphoresThanKmdLimitWhenSignalingFromCpuThenSignalIsSplitIntoCallsWithinLimit) {
    VariableBackup<uint32_t> signalCountBackup(&MockSyncGdi::signalFromCpuCallCount, 0u);
    VariableBackup<MockSyncGdi::CapturedObjects> capturedObjectsBackup(&MockSyncGdi::capturedObjects, {});

    EXPECT_TRUE(wddm->signalExternalSemaphoresFromCpu(operations));

    EXPECT_EQ(expectedCalls, MockSyncGdi::signalFromCpuCallCount);
    expectBatchSplitIntoCallsWithinLimit();
}

TEST_F(WddmExternalSemaphoreBatchTest, givenKmdWaitFailsWhenWaitingOnMoreSemaphoresThanKmdLimitFromCpuThenFalseIsReturnedAndRemainingWaitsAreSkipped) {
    VariableBackup<uint32_t> waitCountBackup(&MockSyncGdi::waitFromCpuCallCount, 0u);
    VariableBackup<MockSyncGdi::CapturedObjects> capturedObjectsBackup(&MockSyncGdi::capturedObjects, {});
    VariableBackup<bool> failWaitBackup(&MockSyncGdi::failWaitForSynchObjectFromCpu, true);

    EXPECT_FALSE(wddm->waitExternalSemaphoresFromCpu(operations));
    EXPECT_EQ(1u, MockSyncGdi::waitFromCpuCallCount);
}

TEST_F(WddmExternalSemaphoreBatchTest, givenKmdSignalFailsWhenSignalingMoreSemaphoresThanKmdLimitFromCpuThenFalseIsReturnedAndRemainingSignalsAreStillAttempted) {
    VariableBackup<uint32_t> signalCountBackup(&MockSyncGdi::signalFromCpuCallCount, 0u);
    VariableBackup<MockSyncGdi::CapturedObjects> capturedObjectsBackup(&MockSyncGdi::capturedObjects, {});
    VariableBackup<bool> failSignalBackup(&MockSyncGdi::failSignalSynchObjectFromCpu, true);

    EXPECT_FALSE(wddm->signalExternalSemaphoresFromCpu(operations));
    EXPECT_EQ(expectedCalls, MockSyncGdi::signalFromCpuCallCount);
}

} // namespace NEO
