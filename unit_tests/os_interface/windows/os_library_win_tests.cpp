/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/os_library.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"

#include "gtest/gtest.h"

#include <memory>

namespace Os {
extern const char *testDllName;
}

using namespace OCLRT;

class OsLibraryBackup : public Windows::OsLibrary {
    using Type = decltype(Windows::OsLibrary::loadLibraryExA);
    using BackupType = typename VariableBackup<Type>;

    using ModuleNameType = decltype(Windows::OsLibrary::getModuleFileNameA);
    using ModuleNameBackupType = typename VariableBackup<ModuleNameType>;

    struct Backup {
        std::unique_ptr<BackupType> bkp1 = nullptr;
        std::unique_ptr<ModuleNameBackupType> bkp2 = nullptr;
    };

  public:
    static std::unique_ptr<Backup> backup(Type newValue, ModuleNameType newModuleName) {
        std::unique_ptr<Backup> bkp(new Backup());
        bkp->bkp1.reset(new BackupType(&OsLibrary::loadLibraryExA, newValue));
        bkp->bkp2.reset(new ModuleNameBackupType(&OsLibrary::getModuleFileNameA, newModuleName));
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

TEST(OSLibraryWinTest, gitOsLibraryWinWhenLoadDependencyFailsThenFallbackToNonDriverStore) {
    auto bkp = OsLibraryBackup::backup(LoadLibraryExAMock, GetModuleFileNameAMock);

    std::unique_ptr<OsLibrary> library(OsLibrary::load(Os::testDllName));
    EXPECT_NE(nullptr, library);
}

TEST(OSLibraryWinTest, gitOsLibraryWinWhenLoadDependencyThenProperPathIsConstructed) {
    auto bkp = OsLibraryBackup::backup(LoadLibraryExAMock, GetModuleFileNameAMock);
    VariableBackup<bool> bkpM(&mockWillFail, false);

    std::unique_ptr<OsLibrary> library(OsLibrary::load(Os::testDllName));
    EXPECT_NE(nullptr, library);
}
