/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
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
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheDir.set("ult\\directory\\");

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, AllPathsExist::statMock);

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvCacheDir.set("ult/directory/");

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvCacheDir.set("ult/directory/");

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvXdgCacheHome.set("xdg/directory/");

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, XDGEnvPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenXdgCachePathWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvXdgCacheHome.set("xdg/directory");

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvHome.set("home/directory/");

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenHomeCachePathWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvHome.set("home/directory");

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSet::statMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenCacheMaxSizeSetTo0WhenGetDefaultConfigThenCacheSizeIsSetToMaxSize) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(0);
    debugManager.flags.EnvCacheDir.set("ult/directory/");

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvHome.set("home/directory/");

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, HomeEnvPathIsSetButDotCacheDoesNotExist::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomeEnvPathIsSetButDotCacheDoesNotExist::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenHomeCachePathWithoutExistingDotCacheWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvHome.set("home/directory");

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvXdgCacheHome.set("xdg/directory/");

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, XdgPathIsSetAndNeedToCreate::statMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

TEST(ClCacheDefaultConfigLinuxTest, GivenXdgEnvWithoutTrailingSlashWhenNeoCompilerCacheNotExistsThenCreateNeoCompilerCacheFolder) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvXdgCacheHome.set("xdg/directory");

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvXdgCacheHome.set("xdg/directory/");
    bool mkdirCalledTemp = false;

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
    DebugManagerStateRestore restorer;
    debugManager.flags.EnvCachePersistent.set(0);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
}

TEST(ClCacheDefaultConfigLinuxTest, GivenIgcEnvVarSetThenCacheConfigRemainsEnabled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(false);
    debugManager.flags.EnvCachePersistent.set(1);
    debugManager.flags.EnvCacheMaxSize.set(22);
    debugManager.flags.EnvCacheDir.set("ult/directory/");

    std::unordered_map<std::string, std::string> mockableEnvs;
    std::vector<std::string> envStorage;

    {
        auto environVec = NEO::ULT::MockEnvironBackup::buildEnvironFromMap(mockableEnvs, envStorage);
        NEO::ULT::MockEnvironBackup mockEnvBackup(environVec.data());
        VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, NEO::ULT::MockEnvironBackup::defaultStatMock);

        auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
        EXPECT_TRUE(cacheConfig.enabled);
        EXPECT_EQ(cacheConfig.cacheDir, "ult/directory/");
    }
    {
        mockableEnvs["IGC_DEBUG"] = "1";
        auto environVec = NEO::ULT::MockEnvironBackup::buildEnvironFromMap(mockableEnvs, envStorage);
        NEO::ULT::MockEnvironBackup mockEnvBackup(environVec.data());
        VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, NEO::ULT::MockEnvironBackup::defaultStatMock);

        auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
        EXPECT_TRUE(cacheConfig.enabled);
        EXPECT_EQ(cacheConfig.cacheDir, "ult/directory/");
    }
}

} // namespace NEO
