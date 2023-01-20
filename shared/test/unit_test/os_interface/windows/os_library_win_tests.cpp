/*
 * Copyright (C) 2018-2022 Intel Corporation
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
    using Type = decltype(Windows::OsLibrary::loadLibraryExA);
    using BackupType = typename VariableBackup<Type>;

    using ModuleNameType = decltype(Windows::OsLibrary::getModuleFileNameA);
    using ModuleNameBackupType = typename VariableBackup<ModuleNameType>;

    using SystemDirectoryType = decltype(Windows::OsLibrary::getSystemDirectoryA);
    using SystemDirectoryBackupType = typename VariableBackup<SystemDirectoryType>;

    struct Backup {
        std::unique_ptr<BackupType> bkp1 = nullptr;
        std::unique_ptr<ModuleNameBackupType> bkp2 = nullptr;
        std::unique_ptr<SystemDirectoryBackupType> bkp3 = nullptr;
    };

  public:
    static std::unique_ptr<Backup> backup(Type newValue, ModuleNameType newModuleName, SystemDirectoryType newSystemDirectoryName) {
        std::unique_ptr<Backup> bkp(new Backup());
        bkp->bkp1.reset(new BackupType(&OsLibrary::loadLibraryExA, newValue));
        bkp->bkp2.reset(new ModuleNameBackupType(&OsLibrary::getModuleFileNameA, newModuleName));
        bkp->bkp3.reset(new SystemDirectoryBackupType(&OsLibrary::getSystemDirectoryA, newSystemDirectoryName));
        return bkp;
    };
};

bool mockWillFail = true;
void trimFileName(char *buff, size_t length) {
    for (size_t l = length; l > 0; l--) {
        if (buff[l - 1] == '\\') {
            buff[l] = '\0';
            break;
        }
    }
}

DWORD WINAPI GetModuleFileNameAMock(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    return snprintf(lpFilename, nSize, "z:\\SomeFakeName.dll");
}

HMODULE WINAPI LoadLibraryExAMock(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags) {
    if (mockWillFail)
        return NULL;

    char fName[MAX_PATH];
    auto lenFn = strlen(lpFileName);
    strcpy_s(fName, sizeof(fName), lpFileName);
    trimFileName(fName, lenFn);

    EXPECT_STREQ("z:\\", fName);

    return (HMODULE)1;
}

UINT WINAPI GetSystemDirectoryAMock(LPSTR lpBuffer, UINT uSize) {
    const char path[] = "C:\\System";
    strcpy_s(lpBuffer, sizeof(path), path);
    return sizeof(path) - 1; // do not include terminating null
}

TEST(OSLibraryWinTest, WhenLoadDependencyFailsThenFallbackToNonDriverStore) {
    auto bkp = OsLibraryBackup::backup(LoadLibraryExAMock, GetModuleFileNameAMock, GetSystemDirectoryAMock);

    std::unique_ptr<OsLibrary> library(OsLibrary::load(Os::testDllName));
    EXPECT_NE(nullptr, library);
}

TEST(OSLibraryWinTest, WhenDependencyLoadsThenProperPathIsConstructed) {
    auto bkp = OsLibraryBackup::backup(LoadLibraryExAMock, GetModuleFileNameAMock, GetSystemDirectoryAMock);
    VariableBackup<bool> bkpM(&mockWillFail, false);

    std::unique_ptr<OsLibrary> library(OsLibrary::load(Os::testDllName));
    EXPECT_NE(nullptr, library);
}

TEST(OSLibraryWinTest, WhenCreatingFullSystemPathThenProperPathIsConstructed) {
    auto bkp = OsLibraryBackup::backup(LoadLibraryExAMock, GetModuleFileNameAMock, GetSystemDirectoryAMock);
    VariableBackup<bool> bkpM(&mockWillFail, false);

    auto fullPath = OsLibrary::createFullSystemPath("test");
    EXPECT_STREQ("C:\\System\\test", fullPath.c_str());
}

TEST(OSLibraryWinTest, GivenInvalidLibraryWhenOpeningLibraryThenLoadLibraryErrorIsReturned) {
    std::string errorValue;
    auto lib = std::make_unique<Windows::OsLibrary>("abc", &errorValue);
    EXPECT_FALSE(errorValue.empty());
}

TEST(OSLibraryWinTest, GivenNoLastErrorOnWindowsThenErrorStringisEmpty) {
    std::string errorValue;

    auto lib = std::make_unique<Windows::OsLibrary>(Os::testDllName, &errorValue);
    EXPECT_NE(nullptr, lib);
    EXPECT_TRUE(errorValue.empty());
    lib->getLastErrorString(&errorValue);
    EXPECT_TRUE(errorValue.empty());
    lib->getLastErrorString(nullptr);
}