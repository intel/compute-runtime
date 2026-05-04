/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/stdio.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/mock_file_io.h"

#include "gtest/gtest.h"

#include <cstdio>

TEST(FileIO, GivenNonEmptyFileWhenCheckingIfHasSizeThenReturnTrue) {
    std::string fileName("fileIO.bin");

    ASSERT_FALSE(virtualFileExists(fileName.c_str()));

    constexpr std::string_view data = "TEST";
    NEO::writeDataToFile(fileName.c_str(), data, false);

    EXPECT_TRUE(virtualFileExists(fileName.c_str()));
    EXPECT_TRUE(NEO::fileExistsHasSize(fileName.c_str()));
    removeVirtualFile(fileName);
}

TEST(FileIO, GivenEmptyFileWhenCheckingIfHasSizeThenReturnFalse) {
    std::string fileName("fileIO.bin");

    ASSERT_FALSE(virtualFileExists(fileName.c_str()));

    constexpr std::string_view data = "";
    NEO::writeDataToFile(fileName.c_str(), data, false);

    EXPECT_TRUE(virtualFileExists(fileName.c_str()));
    EXPECT_FALSE(NEO::fileExistsHasSize(fileName.c_str()));
    removeVirtualFile(fileName);
}

TEST(FileIO, GivenForceLoggingDirectorySetWhenWritingDataThenFileIsCreatedInForcedDirectory) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLoggingDirectory.set("/tmp/forced_dir/");

    std::string fileName("testFile.bin");
    std::string expectedPath("/tmp/forced_dir/testFile.bin");

    ASSERT_FALSE(virtualFileExists(expectedPath.c_str()));

    constexpr std::string_view data = "TEST_DATA";
    NEO::writeDataToFile(fileName.c_str(), data, false);

    EXPECT_TRUE(virtualFileExists(expectedPath.c_str()));
    EXPECT_FALSE(virtualFileExists(fileName.c_str()));

    removeVirtualFile(expectedPath.c_str());
}

TEST(FileIO, GivenForceLoggingDirectoryNotSetWhenWritingDataThenFileIsCreatedWithOriginalName) {
    std::string fileName("testFile.bin");

    ASSERT_FALSE(virtualFileExists(fileName.c_str()));

    constexpr std::string_view data = "TEST_DATA";
    NEO::writeDataToFile(fileName.c_str(), data, false);

    EXPECT_TRUE(virtualFileExists(fileName.c_str()));

    removeVirtualFile(fileName.c_str());
}
