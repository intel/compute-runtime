/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_library_linux.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(OsLibraryTest, WhenCreatingFullSystemPathThenProperPathIsConstructed) {
    auto fullPath = OsLibrary::createFullSystemPath("test");
    EXPECT_STREQ("test", fullPath.c_str());
}

} // namespace NEO
