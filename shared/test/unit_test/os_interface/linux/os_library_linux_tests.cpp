/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_library_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "gtest/gtest.h"

#include <dlfcn.h>

namespace NEO {

namespace SysCalls {

extern int dlOpenFlags;
extern bool dlOpenCalled;
} // namespace SysCalls

TEST(OsLibraryTest, WhenCreatingFullSystemPathThenProperPathIsConstructed) {
    auto fullPath = OsLibrary::createFullSystemPath("test");
    EXPECT_STREQ("test", fullPath.c_str());
}

TEST(OsLibraryTest, GivenDisableDeepBindFlagWhenOpeningLibraryThenRtldDeepBindFlagIsNotPassed) {

    DebugManagerStateRestore restorer;
    VariableBackup<int> dlOpenFlagsBackup{&NEO::SysCalls::dlOpenFlags, 0};
    VariableBackup<bool> dlOpenCalledBackup{&NEO::SysCalls::dlOpenCalled, false};

    DebugManager.flags.DisableDeepBind.set(1);
    auto lib = std::make_unique<Linux::OsLibrary>("_abc.so", nullptr);
    EXPECT_TRUE(NEO::SysCalls::dlOpenCalled);
    EXPECT_EQ(0, NEO::SysCalls::dlOpenFlags & RTLD_DEEPBIND);
}

TEST(OsLibraryTest, GivenInvalidLibraryWhenOpeningLibraryThenDlopenErrorIsReturned) {
    VariableBackup<bool> dlOpenCalledBackup{&NEO::SysCalls::dlOpenCalled, false};

    std::string errorValue;
    auto lib = std::make_unique<Linux::OsLibrary>("_abc.so", &errorValue);
    EXPECT_FALSE(errorValue.empty());
    EXPECT_TRUE(NEO::SysCalls::dlOpenCalled);
}

} // namespace NEO
