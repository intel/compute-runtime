/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/path.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {
namespace SysCallStackMock {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("ult/existing_directory1/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    if (filePath.find("ult/regular_file") != filePath.npos) {
        statbuf->st_mode = S_IFREG;
        return 0;
    }

    if (filePath.find("ult/non_existing_directory/") != filePath.npos) {
        return -1;
    }

    return -1;
}
} // namespace SysCallStackMock

TEST(PathExistsTest, GivenNonExistingDirectoryWhenCallingPathExistsThenReturnFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, SysCallStackMock::statMock);

    std::string directoryPath1 = "ult/non_existing_directory/";
    std::string directoryPath2 = "ult/regular_file";

    EXPECT_FALSE(NEO::pathExists(directoryPath1));
    EXPECT_FALSE(NEO::pathExists(directoryPath2));
}

TEST(PathExistsTest, GivenExistingDirectoryWhenCallingPathExistsThenReturnTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, SysCallStackMock::statMock);

    std::string directoryPath = "ult/existing_directory1/";

    EXPECT_TRUE(NEO::pathExists(directoryPath));
}

} // namespace NEO
