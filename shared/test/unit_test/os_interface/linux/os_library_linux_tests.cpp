/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_library_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

#include <dlfcn.h>

namespace Os {
extern const char *testDllName;
} // namespace Os

namespace NEO {

namespace SysCalls {

extern int dlOpenFlags;
extern bool dlOpenCalled;
} // namespace SysCalls

TEST(OsLibraryTest, GivenValidNameWhenGettingFullPathAndDlinfoFailsThenPathIsEmpty) {
    VariableBackup<decltype(SysCalls::sysCallsDlinfo)> mockDlinfo(&SysCalls::sysCallsDlinfo, [](void *handle, int request, void *info) -> int {
        if (request == RTLD_DI_LINKMAP) {
            return -1;
        }
        return 0;
    });
    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc({Os::testDllName}));
    EXPECT_NE(nullptr, library);
    std::string path = library->getFullPath();
    EXPECT_EQ(0u, path.size());
}

TEST(OsLibraryTest, GivenValidLibNameWhenGettingFullPathThenPathIsNotEmpty) {
    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc({Os::testDllName}));
    EXPECT_NE(nullptr, library);
    std::string path = library->getFullPath();
    EXPECT_NE(0u, path.size());
}

TEST(OsLibraryTest, WhenCreatingFullSystemPathThenProperPathIsConstructed) {
    auto fullPath = OsLibrary::createFullSystemPath("test");
    EXPECT_STREQ("test", fullPath.c_str());
}

TEST(OsLibraryTest, GivenDisableDeepBindFlagWhenOpeningLibraryThenRtldDeepBindFlagIsNotPassed) {

    DebugManagerStateRestore restorer;
    VariableBackup<int> dlOpenFlagsBackup{&NEO::SysCalls::dlOpenFlags, 0};
    VariableBackup<bool> dlOpenCalledBackup{&NEO::SysCalls::dlOpenCalled, false};

    debugManager.flags.DisableDeepBind.set(1);
    OsLibraryCreateProperties properties("_abc.so");
    auto lib = std::make_unique<Linux::OsLibrary>(properties);
    EXPECT_TRUE(NEO::SysCalls::dlOpenCalled);
    EXPECT_EQ(0, NEO::SysCalls::dlOpenFlags & RTLD_DEEPBIND);
}

TEST(OsLibraryTest, GivenInvalidLibraryWhenOpeningLibraryThenDlopenErrorIsReturned) {
    VariableBackup<bool> dlOpenCalledBackup{&NEO::SysCalls::dlOpenCalled, false};

    std::string errorValue;
    OsLibraryCreateProperties properties("_abc.so");
    properties.errorValue = &errorValue;
    auto lib = std::make_unique<Linux::OsLibrary>(properties);
    EXPECT_FALSE(errorValue.empty());
    EXPECT_TRUE(NEO::SysCalls::dlOpenCalled);
}

TEST(OsLibraryTest, GivenLoadFlagsOverwriteWhenOpeningLibraryThenDlOpenIsCalledWithExpectedFlags) {
    VariableBackup<int> dlOpenFlagsBackup{&NEO::SysCalls::dlOpenFlags, 0};
    VariableBackup<bool> dlOpenCalledBackup{&NEO::SysCalls::dlOpenCalled, false};
    VariableBackup<bool> dlOpenCaptureFileNameBackup{&NEO::SysCalls::captureDlOpenFilePath, true};

    auto expectedFlag = RTLD_LAZY | RTLD_GLOBAL;
    OsLibraryCreateProperties properties("_abc.so");
    properties.customLoadFlags = &expectedFlag;
    auto lib = std::make_unique<Linux::OsLibrary>(properties);
    EXPECT_TRUE(NEO::SysCalls::dlOpenCalled);
    EXPECT_EQ(NEO::SysCalls::dlOpenFlags, expectedFlag);
    EXPECT_EQ(properties.libraryName, NEO::SysCalls::dlOpenFilePathPassed);
}

TEST(OsLibraryTest, WhenPerformSelfOpenThenIgnoreFileNameForDlOpenCall) {
    VariableBackup<bool> dlOpenCalledBackup{&NEO::SysCalls::dlOpenCalled, false};
    VariableBackup<bool> dlOpenCaptureFileNameBackup{&NEO::SysCalls::captureDlOpenFilePath, true};

    OsLibraryCreateProperties properties("_abc.so");
    properties.performSelfLoad = false;
    auto lib = std::make_unique<Linux::OsLibrary>(properties);
    EXPECT_TRUE(NEO::SysCalls::dlOpenCalled);
    EXPECT_EQ(properties.libraryName, NEO::SysCalls::dlOpenFilePathPassed);
    NEO::SysCalls::dlOpenCalled = false;

    properties.performSelfLoad = true;
    lib = std::make_unique<Linux::OsLibrary>(properties);
    EXPECT_TRUE(NEO::SysCalls::dlOpenCalled);
    EXPECT_NE(properties.libraryName, NEO::SysCalls::dlOpenFilePathPassed);
    EXPECT_TRUE(NEO::SysCalls::dlOpenFilePathPassed.empty());
}

} // namespace NEO
