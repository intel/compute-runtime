/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/stdio.h"
#include "shared/test/common/helpers/mock_file_io.h"

#include "gtest/gtest.h"

#include <cstdio>

TEST(FileIO, GivenNonEmptyFileWhenCheckingIfHasSizeThenReturnTrue) {
    std::string fileName("fileIO.bin");
    if (virtualFileExists(fileName)) {
        removeVirtualFile(fileName);
    }

    ASSERT_FALSE(virtualFileExists(fileName.c_str()));

    constexpr std::string_view data = "TEST";
    NEO::writeDataToFile(fileName.c_str(), data);

    EXPECT_TRUE(virtualFileExists(fileName.c_str()));
    EXPECT_TRUE(NEO::fileExistsHasSize(fileName.c_str()));
    removeVirtualFile(fileName);
}

TEST(FileIO, GivenEmptyFileWhenCheckingIfHasSizeThenReturnFalse) {
    std::string fileName("fileIO.bin");
    if (virtualFileExists(fileName)) {
        removeVirtualFile(fileName);
    }

    ASSERT_FALSE(virtualFileExists(fileName.c_str()));

    constexpr std::string_view data = "";
    NEO::writeDataToFile(fileName.c_str(), data);

    EXPECT_TRUE(virtualFileExists(fileName.c_str()));
    EXPECT_FALSE(NEO::fileExistsHasSize(fileName.c_str()));
    removeVirtualFile(fileName);
}
