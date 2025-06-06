/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

namespace AllPathsExist {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IFDIR;
    return 0;
}
} // namespace AllPathsExist

TEST(ClCacheDefaultConfigLinuxTest, GivenPrintDebugMessagesWhenCacheIsEnabledThenMessageWithPathIsPrintedToStdout) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(true);

    std::unordered_map<std::string, std::string> mockableEnvs;
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, AllPathsExist::statMock);

    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_DIR"] = "ult\\directory\\";

    StreamCapture capture;
    capture.captureStdout();
    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_STREQ(output.c_str(), "NEO_CACHE_PERSISTENT is enabled. Cache is located in: ult\\directory\\\n\n");
}

namespace AllVariablesCorrectlySet {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("ult/directory/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}
} // namespace AllVariablesCorrectlySet

TEST(ClCacheDefaultConfigLinuxTest, GivenAllEnvVarWhenProperlySetThenProperConfigIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, AllVariablesCorrectlySet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "ult/directory/");
}

namespace NonExistingPathIsSet {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    return -1;
}

} // namespace NonExistingPathIsSet

TEST(ClCacheDefaultConfigLinuxTest, GivenNonExistingPathWhenGetCompilerCacheConfigThenConfigWithDisabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, NonExistingPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
}

namespace XDGEnvPathIsSet {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("xdg/directory/neo_compiler_cache") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    if (filePath.find("xdg/directory") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}
} // namespace XDGEnvPathIsSet

TEST(ClCacheDefaultConfigLinuxTest, GivenXdgCachePathSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, XDGEnvPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenXdgCachePathWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, XDGEnvPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace HomeEnvPathIsSet {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("home/directory/.cache/neo_compiler_cache") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    if (filePath.find("home/directory/.cache/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}

} // namespace HomeEnvPathIsSet

TEST(ClCacheDefaultConfigLinuxTest, GivenHomeCachePathSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenHomeCachePathWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenCacheMaxSizeSetTo0WhenGetDefaultConfigThenCacheSizeIsSetToMaxSize) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "0";
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, AllVariablesCorrectlySet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, std::numeric_limits<size_t>::max());
    EXPECT_EQ(cacheConfig.cacheDir, "ult/directory/");
}

namespace HomeEnvPathIsSetButDotCacheDoesNotExist {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    static bool called = false;

    if (filePath.find("home/directory/.cache/neo_compiler_cache") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    if (filePath.find("home/directory/.cache/") != filePath.npos) {
        if (!called) {
            called = true;
            return -1;
        }

        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}

int mkdirMock(const std::string &dir) {
    return 0;
}
} // namespace HomeEnvPathIsSetButDotCacheDoesNotExist

TEST(ClCacheDefaultConfigLinuxTest, GivenHomeCachePathSetWithoutExistingDotCacheWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSetButDotCacheDoesNotExist::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomeEnvPathIsSetButDotCacheDoesNotExist::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenHomeCachePathWithoutExistingDotCacheWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSetButDotCacheDoesNotExist::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomeEnvPathIsSetButDotCacheDoesNotExist::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

namespace XdgPathIsSetAndNeedToCreate {
int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("xdg/directory/neo_compiler_cache") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    if (filePath.find("xdg/directory") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}

int mkdirMock(const std::string &dir) {
    return 0;
}
} // namespace XdgPathIsSetAndNeedToCreate

TEST(ClCacheDefaultConfigLinuxTest, GivenXdgEnvWhenNeoCompilerCacheNotExistsThenCreateNeoCompilerCacheFolder) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, XdgPathIsSetAndNeedToCreate::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenXdgEnvWithoutTrailingSlashWhenNeoCompilerCacheNotExistsThenCreateNeoCompilerCacheFolder) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, XdgPathIsSetAndNeedToCreate::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace XdgPathIsSetAndOtherProcessCreatesPath {
bool mkdirCalled = false;

int statMock(const std::string &filePath, struct stat *statbuf) noexcept {
    if (filePath.find("xdg/directory/neo_compiler_cache") != filePath.npos) {
        return -1;
    }

    if (filePath.find("xdg/directory/") != filePath.npos) {
        statbuf->st_mode = S_IFDIR;
        return 0;
    }

    return -1;
}

int mkdirMock(const std::string &dir) {
    if (dir.find("xdg/directory/neo_compiler_cache") != dir.npos) {
        mkdirCalled = true;
        errno = EEXIST;
        return -1;
    }
    return 0;
}
} // namespace XdgPathIsSetAndOtherProcessCreatesPath

TEST(ClCacheDefaultConfigLinuxTest, GivenXdgEnvWhenOtherProcessCreatesNeoCompilerCacheFolderThenProperConfigIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";
    bool mkdirCalledTemp = false;

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, XdgPathIsSetAndOtherProcessCreatesPath::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndOtherProcessCreatesPath::mkdirMock);
    VariableBackup<bool> mkdirCalledBackup(&XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled, mkdirCalledTemp);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
    EXPECT_TRUE(XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled);
}

TEST(ClCacheDefaultConfigLinuxTest, GivenNeoCachePersistentSetToZeroWhenGetDefaultCompilerCacheConfigThenCacheIsDisabled) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "0";

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
}
} // namespace NEO
