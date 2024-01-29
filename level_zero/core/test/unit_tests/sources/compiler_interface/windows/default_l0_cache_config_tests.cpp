/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

namespace SysCalls {
extern DWORD getLastErrorResult;

extern bool pathExistsMock;
extern const size_t pathExistsPathsSize;
extern std::string pathExistsPaths[];

extern size_t createDirectoryACalled;
extern BOOL createDirectoryAResult;

extern HRESULT shGetKnownFolderPathResult;
extern const size_t shGetKnownFolderSetPathSize;
extern wchar_t shGetKnownFolderSetPath[];
} // namespace SysCalls

struct L0CompilerCacheTest : public ::testing::Test {
    L0CompilerCacheTest()
        : mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs),
          getLastErrorResultBackup(&SysCalls::getLastErrorResult),
          shGetKnownFolderPathResultBackup(&SysCalls::shGetKnownFolderPathResult),
          createDirectoryACalledBackup(&SysCalls::createDirectoryACalled),
          createDirectoryAResultBackup(&SysCalls::createDirectoryAResult) {}

    void SetUp() override {
        SysCalls::createDirectoryACalled = 0u;
    }

    void TearDown() override {
        std::wmemset(SysCalls::shGetKnownFolderSetPath, 0, SysCalls::shGetKnownFolderSetPathSize);
        for (size_t i = 0; i < SysCalls::pathExistsPathsSize; i++) {
            std::string().swap(SysCalls::pathExistsPaths[i]);
        }
    }

  protected:
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup;
    std::unordered_map<std::string, std::string> mockableEnvs;

    VariableBackup<DWORD> getLastErrorResultBackup;
    VariableBackup<HRESULT> shGetKnownFolderPathResultBackup;
    VariableBackup<size_t> createDirectoryACalledBackup;
    VariableBackup<BOOL> createDirectoryAResultBackup;
    DebugManagerStateRestore restorer;
};

TEST_F(L0CompilerCacheTest, GivenDefaultL0CacheConfigWithPathExistsThenValuesAreProperlyPopulated) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(0);

    bool pathExistsMock = true;
    VariableBackup<bool> pathExistsMockBackup(&NEO::SysCalls::pathExistsMock, pathExistsMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("l0_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".l0_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_TRUE(cacheConfig.enabled);
}

TEST_F(L0CompilerCacheTest, GivenDefaultL0CacheConfigWithNonExistingPathThenValuesAreProperlyPopulated) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(0);

    bool pathExistsMock = false;
    VariableBackup<bool> pathExistsMockBackup(&NEO::SysCalls::pathExistsMock, pathExistsMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("l0_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".l0_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_FALSE(cacheConfig.enabled);
}

TEST_F(L0CompilerCacheTest, GivenAllEnvVarWhenProperlySetThenCorrectConfigIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);
    debugManager.flags.NEO_CACHE_DIR.set("ult\\directory\\");

    bool pathExistsMock = true;
    VariableBackup<bool> pathExistsMockBackup(&NEO::SysCalls::pathExistsMock, pathExistsMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".l0_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "ult\\directory\\");
}

TEST_F(L0CompilerCacheTest, GivenNonExistingPathWhenGetCompilerCacheConfigThenConfigWithDisabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);
    debugManager.flags.NEO_CACHE_DIR.set("ult\\directory\\");

    bool pathExistsMock = false;
    VariableBackup<bool> pathExistsMockBackup(&NEO::SysCalls::pathExistsMock, pathExistsMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
}

TEST_F(L0CompilerCacheTest, GivenLocalAppDataCachePathSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);

    SysCalls::pathExistsPaths[0] = "C:\\Users\\user1\\AppData\\Local\\NEO";
    SysCalls::pathExistsPaths[1] = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".l0_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
}

TEST_F(L0CompilerCacheTest, GivenNeoCacheDirNotSetAndLocalAppDataCachePathNotSetWhenGetCompilerCacheConfigThenConfigWithDisabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);

    SysCalls::shGetKnownFolderPathResult = S_FALSE;

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
}

TEST_F(L0CompilerCacheTest, GivenLocalAppDataSetAndNonExistingNeoDirectoryWhenGetCompilerCacheConfigThenNeoDirectoryIsCreatedAndConfigWithEnabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);

    SysCalls::pathExistsPaths[0] = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";
    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".l0_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(L0CompilerCacheTest, GivenLocalAppDataSetAndNonExistingNeoDirectoryWhenGetCompilerCacheConfigAndNeoDirCreationFailsThenConfigWithDisabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);

    SysCalls::pathExistsPaths[0] = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";
    SysCalls::shGetKnownFolderPathResult = S_OK;
    SysCalls::createDirectoryAResult = FALSE;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(L0CompilerCacheTest, GivenLocalAppDataSetAndNonExistingNeoCompilerCacheDirectoryWhenGetCompilerCacheConfigThenNeoCompilerCacheDirectoryIsCreatedAndConfigWithEnabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);

    SysCalls::pathExistsPaths[0] = "C:\\Users\\user1\\AppData\\Local\\NEO";

    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".l0_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(L0CompilerCacheTest, GivenLocalAppDataSetAndNonExistingNeoCompilerCacheDirectoryWhenGetCompilerCacheConfigAndDirectoryCreationFailsThenConfigWithDisabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);

    SysCalls::pathExistsPaths[0] = "C:\\Users\\user1\\AppData\\Local\\NEO";

    SysCalls::shGetKnownFolderPathResult = S_OK;
    SysCalls::createDirectoryAResult = FALSE;
    SysCalls::getLastErrorResult = ERROR_ALREADY_EXISTS + 1;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(L0CompilerCacheTest, GivenLocalAppDataSetWhenGetCompilerCacheConfigAndNeoCompilerCacheDirectoryAlreadyExistsThenConfigWithEnabledCacheIsReturned) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(22);

    SysCalls::pathExistsPaths[0] = "C:\\Users\\user1\\AppData\\Local\\NEO";

    SysCalls::shGetKnownFolderPathResult = S_OK;
    SysCalls::createDirectoryAResult = FALSE;
    SysCalls::getLastErrorResult = ERROR_ALREADY_EXISTS;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".l0_cache");
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(L0CompilerCacheTest, GivenCacheMaxSizeSetTo0WhenGetDefaultConfigThenCacheSizeIsSetToMaxSize) {
    debugManager.flags.NEO_CACHE_PERSISTENT.set(1);
    debugManager.flags.NEO_CACHE_MAX_SIZE.set(0);
    debugManager.flags.NEO_CACHE_DIR.set("ult\\directory\\");

    SysCalls::pathExistsPaths[0] = "ult\\directory\\";

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".l0_cache");
    EXPECT_EQ(cacheConfig.cacheSize, std::numeric_limits<size_t>::max());
    EXPECT_EQ(cacheConfig.cacheDir, "ult\\directory\\");
}

TEST_F(L0CompilerCacheTest, GivenCachePathExistsAndNoEnvVarsSetWhenGetDefaultCompilerCacheConfigThenCacheIsEnabled) {
    bool pathExistsMock = true;
    VariableBackup<bool> pathExistsMockBackup(&NEO::SysCalls::pathExistsMock, pathExistsMock);

    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ".l0_cache");
    EXPECT_EQ(cacheConfig.cacheSize, static_cast<size_t>(MemoryConstants::gigaByte));
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
}

} // namespace NEO
