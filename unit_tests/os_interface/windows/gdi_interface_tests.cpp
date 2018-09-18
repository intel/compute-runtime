/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if defined(_WIN32)
#include "runtime/os_interface/os_library.h"
#include "runtime/os_interface/windows/gdi_interface.h"

#include "test.h"
#include "gtest/gtest.h"

TEST(GdiInterface, creation) {
    OCLRT::Gdi gdi;
    ASSERT_TRUE(gdi.isInitialized());
}

TEST(GdiInterface, failLoad) {
    const char *oldName = Os::gdiDllName;
    Os::gdiDllName = "surely_not_exists_.dll";

    OCLRT::Gdi gdi;
    EXPECT_FALSE(gdi.isInitialized());

    Os::gdiDllName = oldName;
}

TEST(ThkWrapperTest, givenThkWrapperWhenConstructedThenmFuncIsInitialized) {
    OCLRT::ThkWrapper<false, void *> wrapper;
    EXPECT_EQ(nullptr, wrapper.mFunc);
}
#endif
