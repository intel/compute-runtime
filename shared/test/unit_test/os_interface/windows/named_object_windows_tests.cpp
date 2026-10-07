/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/named_object_windows.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"

#include "gtest/gtest.h"

#include <string>

namespace NEO {

TEST(NamedObjectWindowsNamespaceTest, givenGlobalPrefixedNameWhenResolvingDirectoryThenGlobalBaseNamedObjectsIsUsedAndPrefixIsStripped) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = NamedObjectWindows::getNamedObjectDirectoryPath(4u, L"Global\\myObject", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myObject");
}

TEST(NamedObjectWindowsNamespaceTest, givenGlobalPrefixedNameInSessionZeroWhenResolvingDirectoryThenGlobalBaseNamedObjectsIsUsed) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = NamedObjectWindows::getNamedObjectDirectoryPath(0u, L"Global\\myObject", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myObject");
}

TEST(NamedObjectWindowsNamespaceTest, givenLocalPrefixedNameWithNonZeroSessionWhenResolvingDirectoryThenSessionDirectoryIsUsedAndPrefixIsStripped) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = NamedObjectWindows::getNamedObjectDirectoryPath(3u, L"Local\\myObject", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\Sessions\\3\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myObject");
}

TEST(NamedObjectWindowsNamespaceTest, givenLocalPrefixedNameInSessionZeroWhenResolvingDirectoryThenBaseNamedObjectsIsUsedAndPrefixIsStripped) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = NamedObjectWindows::getNamedObjectDirectoryPath(0u, L"Local\\myObject", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myObject");
}

TEST(NamedObjectWindowsNamespaceTest, givenUnprefixedNameWithNonZeroSessionWhenResolvingDirectoryThenSessionDirectoryIsUsedAndNameIsUnchanged) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = NamedObjectWindows::getNamedObjectDirectoryPath(7u, L"myObject", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\Sessions\\7\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myObject");
}

TEST(NamedObjectWindowsNamespaceTest, givenUnprefixedNameInSessionZeroWhenResolvingDirectoryThenBaseNamedObjectsIsUsedAndNameIsUnchanged) {
    const wchar_t *relativeName = nullptr;
    auto directoryPath = NamedObjectWindows::getNamedObjectDirectoryPath(0u, L"myObject", &relativeName);
    EXPECT_EQ(directoryPath, std::wstring(L"\\BaseNamedObjects"));
    EXPECT_STREQ(relativeName, L"myObject");
}

TEST(NamedObjectWindowsOpenTest, givenInvalidNameWhenOpeningNamedObjectThenNullptrIsReturnedAndNothingIsOpened) {
    VariableBackup<size_t> openDirBackup(&SysCalls::ntOpenDirectoryObjectCalled, 0u);
    uint32_t openFnCalls = 0u;
    auto openFn = [&openFnCalls](OBJECT_ATTRIBUTES *) -> HANDLE {
        openFnCalls++;
        return reinterpret_cast<HANDLE>(0x1);
    };

    const std::wstring tooLongName(NamedObjectWindows::maxNameLengthInWideChars + 1, L'a');

    EXPECT_EQ(nullptr, NamedObjectWindows::openNamedObject(nullptr, false, openFn));
    EXPECT_EQ(nullptr, NamedObjectWindows::openNamedObject(L"", false, openFn));
    EXPECT_EQ(nullptr, NamedObjectWindows::openNamedObject(tooLongName.c_str(), false, openFn));
    EXPECT_EQ(0u, openFnCalls);
    EXPECT_EQ(0u, SysCalls::ntOpenDirectoryObjectCalled);
}

TEST(NamedObjectWindowsOpenTest, givenNameOfMaxLengthWhenOpeningNamedObjectThenObjectIsOpened) {
    const std::wstring maxLengthName(NamedObjectWindows::maxNameLengthInWideChars, L'a');
    uint32_t openFnCalls = 0u;

    auto handle = NamedObjectWindows::openNamedObject(maxLengthName.c_str(), true, [&openFnCalls](OBJECT_ATTRIBUTES *) -> HANDLE {
        openFnCalls++;
        return reinterpret_cast<HANDLE>(0x1);
    });

    EXPECT_EQ(reinterpret_cast<HANDLE>(0x1), handle);
    EXPECT_EQ(1u, openFnCalls);
}

TEST(NamedObjectWindowsOpenTest, givenNameWhenOpeningNamedObjectThenRelativeNameUnderDirectoryIsPassedAndDirectoryIsClosed) {
    VariableBackup<decltype(SysCalls::sysCallsProcessIdToSessionId)> backupSessionId(&SysCalls::sysCallsProcessIdToSessionId, [](DWORD, DWORD *pSessionId) -> BOOL {
        *pSessionId = 2;
        return TRUE;
    });
    VariableBackup<decltype(SysCalls::sysCallsNtOpenDirectoryObject)> backupOpenDir(&SysCalls::sysCallsNtOpenDirectoryObject, [](PHANDLE directoryHandle, ACCESS_MASK, POBJECT_ATTRIBUTES objectAttributes) -> NTSTATUS {
        EXPECT_STREQ(L"\\Sessions\\2\\BaseNamedObjects", objectAttributes->ObjectName->Buffer);
        *directoryHandle = reinterpret_cast<HANDLE>(0x7);
        return STATUS_SUCCESS;
    });
    auto closeHandleCallsBefore = SysCalls::closeHandleCalled;

    std::wstring capturedName;
    HANDLE capturedRoot = nullptr;
    auto handle = NamedObjectWindows::openNamedObject(L"Local\\myObject", false, [&](OBJECT_ATTRIBUTES *objectAttributes) -> HANDLE {
        capturedName.assign(objectAttributes->ObjectName->Buffer);
        capturedRoot = objectAttributes->RootDirectory;
        return reinterpret_cast<HANDLE>(0x9);
    });

    EXPECT_EQ(reinterpret_cast<HANDLE>(0x9), handle);
    EXPECT_EQ(std::wstring(L"myObject"), capturedName);
    EXPECT_EQ(reinterpret_cast<HANDLE>(0x7), capturedRoot);
    EXPECT_EQ(closeHandleCallsBefore + 1u, SysCalls::closeHandleCalled);
}

TEST(NamedObjectWindowsOpenTest, givenForceGlobalWhenOpeningNamedObjectThenSessionIsNotQueriedAndBaseNamedObjectsIsUsed) {
    VariableBackup<decltype(SysCalls::sysCallsProcessIdToSessionId)> backupSessionId(&SysCalls::sysCallsProcessIdToSessionId, [](DWORD, DWORD *pSessionId) -> BOOL {
        ADD_FAILURE();
        *pSessionId = 2;
        return TRUE;
    });
    VariableBackup<decltype(SysCalls::sysCallsNtOpenDirectoryObject)> backupOpenDir(&SysCalls::sysCallsNtOpenDirectoryObject, [](PHANDLE directoryHandle, ACCESS_MASK, POBJECT_ATTRIBUTES objectAttributes) -> NTSTATUS {
        EXPECT_STREQ(L"\\BaseNamedObjects", objectAttributes->ObjectName->Buffer);
        *directoryHandle = reinterpret_cast<HANDLE>(0x7);
        return STATUS_SUCCESS;
    });

    auto handle = NamedObjectWindows::openNamedObject(L"myObject", true, [](OBJECT_ATTRIBUTES *) -> HANDLE {
        return reinterpret_cast<HANDLE>(0x9);
    });
    EXPECT_EQ(reinterpret_cast<HANDLE>(0x9), handle);
}

TEST(NamedObjectWindowsOpenTest, givenOpenDirectoryFailsWhenOpeningNamedObjectThenNullptrIsReturnedAndOpenFnIsNotCalled) {
    VariableBackup<decltype(SysCalls::sysCallsNtOpenDirectoryObject)> backupOpenDir(&SysCalls::sysCallsNtOpenDirectoryObject, [](PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES) -> NTSTATUS {
        return STATUS_UNSUCCESSFUL;
    });

    uint32_t openFnCalls = 0u;
    auto handle = NamedObjectWindows::openNamedObject(L"myObject", false, [&openFnCalls](OBJECT_ATTRIBUTES *) -> HANDLE {
        openFnCalls++;
        return reinterpret_cast<HANDLE>(0x9);
    });
    EXPECT_EQ(nullptr, handle);
    EXPECT_EQ(0u, openFnCalls);
}

TEST(NamedObjectWindowsOpenTest, givenOpenFnFailsWhenOpeningNamedObjectThenNullptrIsReturnedAndDirectoryIsClosed) {
    auto closeHandleCallsBefore = SysCalls::closeHandleCalled;
    auto handle = NamedObjectWindows::openNamedObject(L"myObject", false, [](OBJECT_ATTRIBUTES *) -> HANDLE {
        return nullptr;
    });
    EXPECT_EQ(nullptr, handle);
    EXPECT_EQ(closeHandleCallsBefore + 1u, SysCalls::closeHandleCalled);
}

} // namespace NEO
