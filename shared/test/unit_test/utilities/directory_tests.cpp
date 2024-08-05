/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include "gtest/gtest.h"

namespace NEO {

class DirectoryTest : public Directory {
    using Directory::Directory;

  public:
    std::vector<std::string> createdDirectories;

  protected:
    void create(const std::string &path) override {
        createdDirectories.push_back(path);
    }
};

TEST(DirectoryTest, givenPathAndCreateDirsAndReturnDirsFlagsThenCorrectDirsAreCreatedAndReturned) {
    DirectoryTest dir("./dir1/dir2");
    std::vector<std::string> expectedDirectories{".", "./dir1", "./dir1/dir2"};

    auto returnedDirectories = *dir.parseDirectories(Directory::createDirs | Directory::returnDirs);

    EXPECT_EQ(std::size_t{3}, dir.createdDirectories.size());
    EXPECT_EQ(std::size_t{3}, returnedDirectories.size());

    EXPECT_EQ(expectedDirectories, dir.createdDirectories);
    EXPECT_EQ(expectedDirectories, returnedDirectories);
}

TEST(DirectoryTest, givenPathAndCreateDirsFlagThenCorrectDirsAreCreated) {
    DirectoryTest dir("./dir1/dir2");
    std::vector<std::string> expectedDirectories{".", "./dir1", "./dir1/dir2"};

    auto returnedDirectories = dir.parseDirectories(Directory::createDirs);

    EXPECT_EQ(std::nullopt, returnedDirectories);
    EXPECT_EQ(expectedDirectories, dir.createdDirectories);
}

TEST(DirectoryTest, givenPathAndReturnDirsFlagThenCorrectDirsAreReturned) {
    DirectoryTest dir("./dir1/dir2");
    std::vector<std::string> expectedDirectories{".", "./dir1", "./dir1/dir2"};

    auto returnedDirectories = *dir.parseDirectories(Directory::returnDirs);

    EXPECT_EQ(std::size_t{0}, dir.createdDirectories.size());
    EXPECT_EQ(expectedDirectories, returnedDirectories);
}

} // namespace NEO