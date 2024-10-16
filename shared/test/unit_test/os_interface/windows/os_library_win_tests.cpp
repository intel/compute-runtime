/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_library_win.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <memory>

namespace Os {
extern const char *testDllName;
}

using namespace NEO;

class OsLibraryBackup : public Windows::OsLibrary {
  public:
    using Windows::OsLibrary::getModuleHandleA;
    using Windows::OsLibrary::loadLibraryExA;

    using Type = decltype(Windows::OsLibrary::loadLibraryExA);
    using BackupType = VariableBackup<Type>;

    using ModuleNameType = decltype(Windows::OsLibrary::getModuleFileNameA);
    using ModuleNameBackupType = VariableBackup<ModuleNameType>;

    using SystemDirectoryType = decltype(Windows::OsLibrary::getSystemDirectoryA);
    using SystemDirectoryBackupType = VariableBackup<SystemDirectoryType>;

    struct Backup {
        std::unique_ptr<BackupType> bkp1 = nullptr;
        std::unique_ptr<ModuleNameBackupType> bkp2 = nullptr;
        std::unique_ptr<SystemDirectoryBackupType> bkp3 = nullptr;
    };

    static std::unique_ptr<Backup> backup(Type newValue, ModuleNameType newModuleName, SystemDirectoryType newSystemDirectoryName) {
        std::unique_ptr<Backup> bkp(new Backup());
        bkp->bkp1.reset(new BackupType(&OsLibrary::loadLibraryExA, newValue));
        bkp->bkp2.reset(new ModuleNameBackupType(&OsLibrary::getModuleFileNameA, newModuleName));
        bkp->bkp3.reset(new SystemDirectoryBackupType(&OsLibrary::getSystemDirectoryA, newSystemDirectoryName));
        return bkp;
    };
};

bool mockWillFailInNonSystem32 = true;
void trimFileName(char *buff, size_t length) {
    for (size_t l = length; l > 0; l--) {
        if (buff[l - 1] == '\\') {
            buff[l] = '\0';
            break;
        }
    }
}

DWORD WINAPI getModuleFileNameAMock(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    return snprintf(lpFilename, nSize, "z:\\SomeFakeName.dll");
}

HMODULE WINAPI loadLibraryExAMock(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags) {
    if (mockWillFailInNonSystem32 && dwFlags != LOAD_LIBRARY_SEARCH_SYSTEM32)
        return NULL;

    char fName[MAX_PATH];
    auto lenFn = strlen(lpFileName);
    strcpy_s(fName, sizeof(fName), lpFileName);
    trimFileName(fName, lenFn);

    if (dwFlags != LOAD_LIBRARY_SEARCH_SYSTEM32) {
        EXPECT_STREQ("z:\\", fName);
    }

    return (HMODULE)1;
}

UINT WINAPI getSystemDirectoryAMock(LPSTR lpBuffer, UINT uSize) {
    const char path[] = "C:\\System";
    strcpy_s(lpBuffer, sizeof(path), path);
    return sizeof(path) - 1; // do not include terminating null
}

TEST(OSLibraryWinTest, WhenLoadDependencyFailsThenFallbackToSystem32) {
    auto bkp = OsLibraryBackup::backup(loadLibraryExAMock, getModuleFileNameAMock, getSystemDirectoryAMock);

    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc({Os::testDllName}));
    EXPECT_NE(nullptr, library);
}

TEST(OSLibraryWinTest, WhenDependencyLoadsThenProperPathIsConstructed) {
    auto bkp = OsLibraryBackup::backup(loadLibraryExAMock, getModuleFileNameAMock, getSystemDirectoryAMock);
    VariableBackup<bool> bkpM(&mockWillFailInNonSystem32, false);

    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc({Os::testDllName}));
    EXPECT_NE(nullptr, library);
}

TEST(OSLibraryWinTest, WhenCreatingFullSystemPathThenProperPathIsConstructed) {
    auto bkp = OsLibraryBackup::backup(loadLibraryExAMock, getModuleFileNameAMock, getSystemDirectoryAMock);
    VariableBackup<bool> bkpM(&mockWillFailInNonSystem32, false);

    auto fullPath = OsLibrary::createFullSystemPath("test");
    EXPECT_STREQ("C:\\System\\test", fullPath.c_str());
}

TEST(OSLibraryWinTest, GivenInvalidLibraryWhenOpeningLibraryThenLoadLibraryErrorIsReturned) {
    std::string errorValue;
    OsLibraryCreateProperties properties("abc");
    properties.errorValue = &errorValue;
    auto lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_FALSE(errorValue.empty());
}

TEST(OSLibraryWinTest, GivenNoLastErrorOnWindowsThenErrorStringisEmpty) {
    std::string errorValue;

    OsLibraryCreateProperties properties(Os::testDllName);
    properties.errorValue = &errorValue;
    auto lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_NE(nullptr, lib);
    EXPECT_TRUE(errorValue.empty());
    lib->getLastErrorString(&errorValue);
    EXPECT_TRUE(errorValue.empty());
    lib->getLastErrorString(nullptr);
}

TEST(OSLibraryWinTest, WhenCreateOsLibraryWithSelfOpenThenDontLoadLibrary) {
    OsLibraryCreateProperties properties(Os::testDllName);
    VariableBackup<decltype(OsLibraryBackup::loadLibraryExA)> backupLoadLibrary(&OsLibraryBackup::loadLibraryExA, [](LPCSTR, HANDLE, DWORD) -> HMODULE {
        UNRECOVERABLE_IF(true);
        return nullptr;
    });

    VariableBackup<decltype(OsLibraryBackup::getModuleHandleA)> backupGetModuleHandle(&OsLibraryBackup::getModuleHandleA, [](LPCSTR moduleName) -> HMODULE {
        return nullptr;
    });
    properties.performSelfLoad = true;
    auto lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_FALSE(lib->isLoaded());
    OsLibraryBackup::getModuleHandleA = [](LPCSTR moduleName) -> HMODULE {
        return reinterpret_cast<HMODULE>(0x1000);
    };
    lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_TRUE(lib->isLoaded());
}