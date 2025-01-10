/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_library_win.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <memory>

namespace Os {
extern const char *testDllName;
}

using namespace NEO;

class OsLibraryBackup : public Windows::OsLibrary {
  public:
    using Windows::OsLibrary::freeLibrary;
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

TEST(OSLibraryWinTest, WhenCreateOsLibraryWithSelfOpenThenDontLoadLibraryOrFreeLibrary) {
    OsLibraryCreateProperties properties(Os::testDllName);
    VariableBackup<decltype(OsLibraryBackup::loadLibraryExA)> backupLoadLibrary(&OsLibraryBackup::loadLibraryExA, [](LPCSTR, HANDLE, DWORD) -> HMODULE {
        UNRECOVERABLE_IF(true);
        return nullptr;
    });

    VariableBackup<decltype(OsLibraryBackup::getModuleHandleA)> backupGetModuleHandle(&OsLibraryBackup::getModuleHandleA, [](LPCSTR moduleName) -> HMODULE {
        return nullptr;
    });

    VariableBackup<decltype(OsLibraryBackup::freeLibrary)> backupFreeLibrary(&OsLibraryBackup::freeLibrary, [](HMODULE) -> BOOL {
        UNRECOVERABLE_IF(true);
        return FALSE;
    });

    properties.performSelfLoad = true;
    auto lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_FALSE(lib->isLoaded());
    OsLibraryBackup::getModuleHandleA = [](LPCSTR moduleName) -> HMODULE {
        return reinterpret_cast<HMODULE>(0x1000);
    };
    lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_TRUE(lib->isLoaded());

    properties.libraryName.clear();
    lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_TRUE(lib->isLoaded());
}

TEST(OSLibraryWinTest, WhenCreateOsLibraryWithoutSelfOpenThenLoadAndFreeLibrary) {
    static uint32_t loadLibraryCalled = 0;
    static uint32_t freeLibraryCalled = 0;
    VariableBackup<decltype(loadLibraryCalled)> backupLoadLibraryCalled(&loadLibraryCalled, 0);
    VariableBackup<decltype(freeLibraryCalled)> backupFreeLibraryCalled(&freeLibraryCalled, 0);

    static HMODULE hModule = reinterpret_cast<HMODULE>(0x1000);

    OsLibraryCreateProperties properties(Os::testDllName);
    VariableBackup<decltype(OsLibraryBackup::loadLibraryExA)> backupLoadLibrary(&OsLibraryBackup::loadLibraryExA, [](LPCSTR, HANDLE, DWORD) -> HMODULE {
        loadLibraryCalled++;
        return hModule;
    });

    VariableBackup<decltype(OsLibraryBackup::getModuleHandleA)> backupGetModuleHandle(&OsLibraryBackup::getModuleHandleA, [](LPCSTR moduleName) -> HMODULE {
        UNRECOVERABLE_IF(true);
        return nullptr;
    });

    VariableBackup<decltype(OsLibraryBackup::freeLibrary)> backupFreeLibrary(&OsLibraryBackup::freeLibrary, [](HMODULE input) -> BOOL {
        EXPECT_EQ(hModule, input);
        freeLibraryCalled++;
        return FALSE;
    });

    properties.performSelfLoad = false;
    auto lib = std::make_unique<Windows::OsLibrary>(properties);
    EXPECT_TRUE(lib->isLoaded());
    EXPECT_EQ(1u, loadLibraryCalled);
    EXPECT_EQ(0u, freeLibraryCalled);
    lib.reset();
    EXPECT_EQ(1u, freeLibraryCalled);
}

class GetLoadedLibVersionWinTest : public ::testing::Test {
  protected:
    struct LibModule {
        struct {
            struct {
                int wasCalledCount;
                DWORD dwFlags;
                LPCWSTR lpModuleName;
                HMODULE *phModule;

                bool forceFail = false;
            } getModuleHandleExW = {};

            struct {
                int wasCalledCount;
                HMODULE hModule;
                LPWSTR lpFilename;
                DWORD nSize;

                bool forceFail = false;
            } getModuleFileNameW = {};

            struct {
                int wasCalledCount;
                LPCWSTR lptstrFilename;
                LPDWORD lpdwHandle;

                bool forceFail = false;
            } getFileVersionInfoSizeW = {};

            struct {
                int wasCalledCount;
                LPCWSTR lptstrFilename;
                DWORD dwHandle;
                DWORD dwLen;
                LPVOID lpData;

                bool forceFail = false;
            } getFileVersionInfoW = {};

            struct {
                int wasCalledCount;
                LPCVOID pBlock;
                std::optional<std::wstring> lpSubBlock;
                LPVOID *lplpBuffer;
                PUINT puLen;

                int forceFailId = -1;
                struct LangCodePage {
                    WORD lang;
                    WORD codePage;
                };
                LangCodePage langCodePages[4] = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
                std::map<std::wstring, std::wstring> versionPerCodepage = {
                    {L"\\StringFileInfo\\00010002\\ProductVersion", L"vvvvvvvvvv"},
                    {L"\\StringFileInfo\\00030004\\ProductVersion", L"vvvvvvvvvv"},
                    {L"\\StringFileInfo\\00050006\\ProductVersion", L"2024.4.1-16618-xxxxxx"},
                    {L"\\StringFileInfo\\00070008\\ProductVersion", L"vvvvvvvvvv"}};
            } verQueryValueW = {};
        } settings = {};
    };

    static inline LibModule *mod = nullptr;
    LibModule tmpMod;

    static inline constexpr std::string_view nonexistenDllName = "nonexistent.dll";

    void SetUp() override {
        mod = &tmpMod;
        SysCalls::sysCallsGetModuleHandleExW = +[](DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) -> BOOL {
            ++mod->settings.getModuleHandleExW.wasCalledCount;
            mod->settings.getModuleHandleExW.dwFlags = dwFlags;
            mod->settings.getModuleHandleExW.lpModuleName = lpModuleName;
            mod->settings.getModuleHandleExW.phModule = phModule;

            auto moduleName = std::wstring(lpModuleName);
            if (mod->settings.getModuleHandleExW.forceFail) {
                return FALSE;
            }
            *phModule = reinterpret_cast<HMODULE>(&mod);
            return TRUE;
        };

        SysCalls::sysCallsGetModuleFileNameW = +[](HMODULE hModule, LPWSTR lpFilename, DWORD nSize) -> DWORD {
            ++mod->settings.getModuleFileNameW.wasCalledCount;
            mod->settings.getModuleFileNameW.hModule = hModule;
            mod->settings.getModuleFileNameW.lpFilename = lpFilename;
            mod->settings.getModuleFileNameW.nSize = nSize;

            if (mod->settings.getModuleFileNameW.forceFail) {
                return 0;
            }

            std::wstring path = L"Z:\\some_dir\\some.dll";
            wmemcpy_s(lpFilename, nSize, path.c_str(), path.size());
            return static_cast<DWORD>(path.size());
        };
        SysCalls::sysCallsGetFileVersionInfoSizeW = +[](LPCWSTR lptstrFilename, LPDWORD lpdwHandle) -> DWORD {
            ++mod->settings.getFileVersionInfoSizeW.wasCalledCount;
            mod->settings.getFileVersionInfoSizeW.lptstrFilename = lptstrFilename;
            mod->settings.getFileVersionInfoSizeW.lpdwHandle = lpdwHandle;

            if (mod->settings.getFileVersionInfoSizeW.forceFail) {
                return 0;
            }

            return 4096U;
        };
        SysCalls::sysCallsGetFileVersionInfoW = +[](LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) -> BOOL {
            ++mod->settings.getFileVersionInfoW.wasCalledCount;
            mod->settings.getFileVersionInfoW.lptstrFilename = lptstrFilename;
            mod->settings.getFileVersionInfoW.dwHandle = dwHandle;
            mod->settings.getFileVersionInfoW.dwLen = dwLen;
            mod->settings.getFileVersionInfoW.lpData = lpData;

            if (mod->settings.getFileVersionInfoW.forceFail) {
                return FALSE;
            }

            return TRUE;
        };
        SysCalls::sysCallsVerQueryValueW = +[](LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen) -> BOOL {
            ++mod->settings.verQueryValueW.wasCalledCount;
            mod->settings.verQueryValueW.pBlock = pBlock;
            mod->settings.verQueryValueW.lpSubBlock = lpSubBlock;
            mod->settings.verQueryValueW.lplpBuffer = lplpBuffer;
            mod->settings.verQueryValueW.puLen = puLen;

            if (mod->settings.verQueryValueW.forceFailId == mod->settings.verQueryValueW.wasCalledCount) {
                return FALSE;
            }

            if (std::wstring(lpSubBlock) == L"\\VarFileInfo\\Translation") {
                *puLen = sizeof(mod->settings.verQueryValueW.langCodePages);
                *lplpBuffer = mod->settings.verQueryValueW.langCodePages;
                return TRUE;
            }

            if (mod->settings.verQueryValueW.versionPerCodepage.find(lpSubBlock) == mod->settings.verQueryValueW.versionPerCodepage.end()) {
                return FALSE;
            }

            *lplpBuffer = const_cast<void *>(reinterpret_cast<const void *>(mod->settings.verQueryValueW.versionPerCodepage[lpSubBlock].c_str()));
            return TRUE;
        };
    }

    VariableBackup<decltype(NEO::SysCalls::sysCallsGetModuleHandleExW)> backupGetModuleHandleExW{&NEO::SysCalls::sysCallsGetModuleHandleExW};
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetModuleFileNameW)> backupGetModuleFileNameW{&NEO::SysCalls::sysCallsGetModuleFileNameW};
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetFileVersionInfoSizeW)> backupGetFileVersionInfoSizeW{&NEO::SysCalls::sysCallsGetFileVersionInfoSizeW};
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetFileVersionInfoW)> backupGetFileVersionInfoW{&NEO::SysCalls::sysCallsGetFileVersionInfoW};
    VariableBackup<decltype(NEO::SysCalls::sysCallsVerQueryValueW)> backupVerQueryValueW{&NEO::SysCalls::sysCallsVerQueryValueW};

    VariableBackup<decltype(GetLoadedLibVersionWinTest::mod)> backupMod{&GetLoadedLibVersionWinTest::mod};
};

TEST_F(GetLoadedLibVersionWinTest, WhenModuleWasNotLoadedThenReturnError) {
    std::string version;
    std::string err;
    mod->settings.getModuleHandleExW.forceFail = true;
    auto ret = NEO::getLoadedLibVersion("test.dll", ".*", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());
    EXPECT_EQ(1, mod->settings.getModuleHandleExW.wasCalledCount);
    EXPECT_EQ(0, mod->settings.getModuleFileNameW.wasCalledCount);
}

TEST_F(GetLoadedLibVersionWinTest, WhenQueryingForModuleHandleThenDontIncreaseRefcount) {
    std::string version;
    std::string err;
    NEO::getLoadedLibVersion("test.dll", ".*", version, err);
    EXPECT_EQ(1, mod->settings.getModuleHandleExW.wasCalledCount);
    EXPECT_EQ(static_cast<DWORD>(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT), mod->settings.getModuleHandleExW.dwFlags);
}

TEST_F(GetLoadedLibVersionWinTest, WhenGetModuleFilenameFailedThenReturnError) {
    std::string version;
    std::string err;
    mod->settings.getModuleFileNameW.forceFail = true;
    auto ret = NEO::getLoadedLibVersion("test.dll", ".*", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());
    EXPECT_EQ(1, mod->settings.getModuleHandleExW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getModuleFileNameW.wasCalledCount);
    EXPECT_EQ(0, mod->settings.getFileVersionInfoSizeW.wasCalledCount);
}

TEST_F(GetLoadedLibVersionWinTest, WhenGetFileVersionInfoSizeWFailedThenReturnError) {
    std::string version;
    std::string err;
    mod->settings.getFileVersionInfoSizeW.forceFail = true;
    auto ret = NEO::getLoadedLibVersion("test.dll", ".*", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());
    EXPECT_EQ(1, mod->settings.getModuleHandleExW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getModuleFileNameW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getFileVersionInfoSizeW.wasCalledCount);
    EXPECT_EQ(0, mod->settings.getFileVersionInfoW.wasCalledCount);
}

TEST_F(GetLoadedLibVersionWinTest, WhenGetFileVersionInfoFailedThenReturnError) {
    std::string version;
    std::string err;
    mod->settings.getFileVersionInfoW.forceFail = true;
    auto ret = NEO::getLoadedLibVersion("test.dll", ".*", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());
    EXPECT_EQ(1, mod->settings.getModuleHandleExW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getModuleFileNameW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getFileVersionInfoSizeW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getFileVersionInfoW.wasCalledCount);
    EXPECT_EQ(0, mod->settings.verQueryValueW.wasCalledCount);
}

TEST_F(GetLoadedLibVersionWinTest, WhenVerQueryValueWFailedThenReturnError) {
    std::string version;
    std::string err;
    mod->settings.verQueryValueW.forceFailId = 1;
    auto ret = NEO::getLoadedLibVersion("test.dll", ".*", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());
    EXPECT_EQ(1, mod->settings.getModuleHandleExW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getModuleFileNameW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getFileVersionInfoSizeW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.getFileVersionInfoW.wasCalledCount);
    EXPECT_EQ(1, mod->settings.verQueryValueW.wasCalledCount);
}

TEST_F(GetLoadedLibVersionWinTest, WhenQueryingForFileVersionInfoThenProperSizeIsUsed) {
    std::string version;
    std::string err;
    mod->settings.verQueryValueW.forceFailId = 1;
    NEO::getLoadedLibVersion("test.dll", ".*", version, err);

    EXPECT_EQ(static_cast<DWORD>(4096), mod->settings.getFileVersionInfoW.dwLen);
}

TEST_F(GetLoadedLibVersionWinTest, WhenQueryingForVersionThenFirstCheckAvailableTranslations) {
    std::string version;
    std::string err;
    mod->settings.verQueryValueW.forceFailId = 1;
    auto ret = NEO::getLoadedLibVersion("test.dll", ".*", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());

    EXPECT_EQ(1, mod->settings.verQueryValueW.wasCalledCount);
    EXPECT_EQ(L"\\VarFileInfo\\Translation", mod->settings.verQueryValueW.lpSubBlock);
}

TEST_F(GetLoadedLibVersionWinTest, WhenFailedToQueryVersionForAvailableTranslationThenReturnError) {
    std::string version;
    std::string err;
    mod->settings.verQueryValueW.forceFailId = 3;
    auto ret = NEO::getLoadedLibVersion("test.dll", "aaaaaaa", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());

    EXPECT_EQ(mod->settings.verQueryValueW.forceFailId, mod->settings.verQueryValueW.wasCalledCount);
}

TEST_F(GetLoadedLibVersionWinTest, WhenNoMatchingVersionFoundThenReturnError) {
    std::string version;
    std::string err;
    auto ret = NEO::getLoadedLibVersion("test.dll", "veryspecificversionpattern", version, err);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(version.empty()) << version;
    EXPECT_FALSE(err.empty());

    EXPECT_EQ(1 + int(mod->settings.verQueryValueW.versionPerCodepage.size()), mod->settings.verQueryValueW.wasCalledCount);
}

TEST_F(GetLoadedLibVersionWinTest, WhenMatchingVersionFoundThenReturnIt) {
    std::string version;
    std::string err;
    auto ret = NEO::getLoadedLibVersion("test.dll", "^([0-9]{4})\\.([-0-9])+\\.([-0-9])+-([-0-9])+.*", version, err);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(version.empty());
    EXPECT_TRUE(err.empty()) << err;
    EXPECT_STREQ("2024.4.1-16618-xxxxxx", version.c_str());
}
