/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if defined(_WIN32)
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

TEST(GdiInterface, WhenGdiIsCreatedThenItIsInitialized) {
    NEO::Gdi gdi;
    ASSERT_TRUE(gdi.isInitialized());
}

TEST(GdiInterface, GivenInvalidGdiDllNameWhenCreatingGdiThenGdiIsNotInitialized) {
    const char *oldName = Os::gdiDllName;
    Os::gdiDllName = "surely_not_exists_.dll";

    NEO::Gdi gdi;
    EXPECT_FALSE(gdi.isInitialized());

    Os::gdiDllName = oldName;
}

TEST(GdiInterface, givenGdiOverridePathWhenGdiInterfaceIsCalledThenOverridePathIsUsed) {
    const char *oldName = Os::gdiDllName;

    DebugManagerStateRestore dbgRestorer;

    Os::gdiDllName = "surely_not_exists_.dll";
    DebugManager.flags.OverrideGdiPath.set(oldName);

    NEO::Gdi gdi;
    EXPECT_TRUE(gdi.isInitialized());

    Os::gdiDllName = oldName;
}

TEST(ThkWrapperTest, givenThkWrapperWhenConstructedThenmFuncIsInitialized) {
    NEO::ThkWrapper<void *> wrapper;
    EXPECT_EQ(nullptr, wrapper.mFunc);
}
#endif
