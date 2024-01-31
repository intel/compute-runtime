/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

namespace SysCalls {
extern bool pathExistsMock;
}

namespace LegacyPathWorksIfNewEnvIsSetToDisabled {
bool pathExistsMock(const std::string &path) {
    if (path.find("cl_cache") != path.npos)
        return true;

    return false;
}
} // namespace LegacyPathWorksIfNewEnvIsSetToDisabled

TEST(CompilerCache, GivenDefaultClCacheConfigWithPathExistsAndNewEnvSetToDisabledThenValuesAreProperlyPopulated) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "0";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, LegacyPathWorksIfNewEnvIsSetToDisabled::pathExistsMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("cl_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".cl_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_TRUE(cacheConfig.enabled);
}

namespace NewEnvIsDisabledAndLegacyPathDoesNotExist {
bool pathExistsMock(const std::string &path) {
    return false;
}
} // namespace NewEnvIsDisabledAndLegacyPathDoesNotExist

TEST(CompilerCache, GivenDefaultClCacheConfigWithNotExistingPathAndNewEnvSetToDisabledThenValuesAreProperlyPopulated) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "0";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, NewEnvIsDisabledAndLegacyPathDoesNotExist::pathExistsMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("cl_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".cl_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_FALSE(cacheConfig.enabled);
}

namespace AllVariablesCorrectlySet {
bool pathExistsMock(const std::string &path) {
    if (path.find("ult/directory/") != path.npos)
        return true;

    return false;
}
} // namespace AllVariablesCorrectlySet

TEST(CompilerCache, GivenAllEnvVarWhenProperlySetThenProperConfigIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, AllVariablesCorrectlySet::pathExistsMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "ult/directory/");
}

namespace NonExistingPathIsSet {
bool pathExistsMock(const std::string &path) {
    return false;
}
} // namespace NonExistingPathIsSet

TEST(CompilerCache, GivenNonExistingPathWhenGetCompilerCacheConfigThenConfigWithDisabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, NonExistingPathIsSet::pathExistsMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
}

namespace XDGEnvPathIsSet {
bool pathExistsMock(const std::string &path) {
    if (path.find("xdg/directory/neo_compiler_cache") != path.npos) {
        return true;
    }
    if (path.find("xdg/directory") != path.npos) {
        return true;
    }
    return false;
}
} // namespace XDGEnvPathIsSet

TEST(CompilerCache, GivenXdgCachePathSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XDGEnvPathIsSet::pathExistsMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

TEST(CompilerCache, GivenXdgCachePathWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XDGEnvPathIsSet::pathExistsMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace HomeEnvPathIsSet {
bool pathExistsMock(const std::string &path) {
    if (path.find("home/directory/.cache/neo_compiler_cache") != path.npos) {
        return true;
    }
    if (path.find("home/directory/.cache/") != path.npos) {
        return true;
    }
    return false;
}
} // namespace HomeEnvPathIsSet

TEST(CompilerCache, GivenHomeCachePathSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomeEnvPathIsSet::pathExistsMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(CompilerCache, GivenHomeCachePathWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomeEnvPathIsSet::pathExistsMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(CompilerCache, GivenCacheMaxSizeSetTo0WhenGetDefaultConfigThenCacheSizeIsSetToMaxSize) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "0";
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, AllVariablesCorrectlySet::pathExistsMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, std::numeric_limits<size_t>::max());
    EXPECT_EQ(cacheConfig.cacheDir, "ult/directory/");
}

namespace HomeEnvPathIsSetButDotCacheDoesNotExist {
bool pathExistsMock(const std::string &path) {
    static bool called = false;
    if (path.find("home/directory/.cache/neo_compiler_cache") != path.npos) {
        return true;
    }
    if (path.find("home/directory/.cache/") != path.npos) {
        bool result = called;
        called = true;
        return result;
    }
    return false;
}
int mkdirMock(const std::string &dir) {
    return 0;
}
} // namespace HomeEnvPathIsSetButDotCacheDoesNotExist

TEST(CompilerCache, GivenHomeCachePathSetWithoutExistingDotCacheWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomeEnvPathIsSetButDotCacheDoesNotExist::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomeEnvPathIsSetButDotCacheDoesNotExist::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

TEST(CompilerCache, GivenHomeCachePathWithoutExistingDotCacheWithoutTrailingSlashSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["HOME"] = "home/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, HomeEnvPathIsSetButDotCacheDoesNotExist::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, HomeEnvPathIsSetButDotCacheDoesNotExist::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "home/directory/.cache/neo_compiler_cache");
}

namespace XdgPathIsSetAndNeedToCreate {
bool pathExistsMock(const std::string &path) {
    if (path.find("xdg/directory/neo_compiler_cache") != path.npos) {
        return false;
    }
    if (path.find("xdg/directory") != path.npos) {
        return true;
    }
    return false;
}
int mkdirMock(const std::string &dir) {
    return 0;
}
} // namespace XdgPathIsSetAndNeedToCreate

TEST(CompilerCache, GivenXdgEnvWhenNeoCompilerCacheNotExistsThenCreateNeoCompilerCacheFolder) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XdgPathIsSetAndNeedToCreate::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

TEST(CompilerCache, GivenXdgEnvWithoutTrailingSlashWhenNeoCompilerCacheNotExistsThenCreateNeoCompilerCacheFolder) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory";

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XdgPathIsSetAndNeedToCreate::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndNeedToCreate::mkdirMock);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
}

namespace XdgPathIsSetAndOtherProcessCreatesPath {
bool mkdirCalled = false;

bool pathExistsMock(const std::string &path) {
    if (path.find("xdg/directory/neo_compiler_cache") != path.npos) {
        return false;
    }
    if (path.find("xdg/directory/") != path.npos) {
        return true;
    }
    return false;
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

TEST(CompilerCache, GivenXdgEnvWhenOtherProcessCreatesNeoCompilerCacheFolderThenProperConfigIsReturned) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["XDG_CACHE_HOME"] = "xdg/directory/";
    bool mkdirCalledTemp = false;

    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&NEO::IoFunctions::mockableEnvValues, &mockableEnvs);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPathExists)> pathExistsBackup(&NEO::SysCalls::sysCallsPathExists, XdgPathIsSetAndOtherProcessCreatesPath::pathExistsMock);
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mkdirBackup(&NEO::SysCalls::sysCallsMkdir, XdgPathIsSetAndOtherProcessCreatesPath::mkdirMock);
    VariableBackup<bool> mkdirCalledBackup(&XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled, mkdirCalledTemp);

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".cl_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "xdg/directory/neo_compiler_cache");
    EXPECT_TRUE(XdgPathIsSetAndOtherProcessCreatesPath::mkdirCalled);
}
} // namespace NEO
