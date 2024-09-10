/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/path.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

namespace SysCalls {
extern size_t getFileAttributesCalled;
extern std::unordered_map<std::string, DWORD> pathAttributes;
} // namespace SysCalls

struct PathExistsTest : public ::testing::Test {
    PathExistsTest()
        : getFileAttributesCalledBackup(&SysCalls::getFileAttributesCalled) {}

    void SetUp() override {
        SysCalls::getFileAttributesCalled = 0u;
    }
    void TearDown() override {
        SysCalls::pathAttributes.clear();
    }

  protected:
    VariableBackup<size_t> getFileAttributesCalledBackup;
};

TEST_F(PathExistsTest, GivenNonExistingDirectoryWhenCallingPathExistsThenReturnFalse) {
    std::string directoryPath = "C:\\non_existing_directory";

    EXPECT_FALSE(NEO::pathExists(directoryPath));
    EXPECT_EQ(1u, SysCalls::getFileAttributesCalled);
}

TEST_F(PathExistsTest, GivenExistingDirectoryWhenCallingPathExistsThenReturnTrue) {
    std::string directoryPath1 = "C:\\existing_directory1";
    std::string directoryPath2 = "C:\\existing_directory2";

    SysCalls::pathAttributes[directoryPath1] = FILE_ATTRIBUTE_DIRECTORY;                                                 // 16
    SysCalls::pathAttributes[directoryPath2] = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY; // 22

    EXPECT_TRUE(NEO::pathExists(directoryPath1));
    EXPECT_TRUE(NEO::pathExists(directoryPath2));

    EXPECT_EQ(2u, SysCalls::getFileAttributesCalled);
}

} // namespace NEO
