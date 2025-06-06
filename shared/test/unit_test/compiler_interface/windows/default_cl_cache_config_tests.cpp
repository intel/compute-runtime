/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

namespace SysCalls {
extern DWORD getLastErrorResult;

extern size_t createDirectoryACalled;
extern BOOL createDirectoryAResult;

extern HRESULT shGetKnownFolderPathResult;
extern const size_t shGetKnownFolderSetPathSize;
extern wchar_t shGetKnownFolderSetPath[];

extern DWORD getFileAttributesResult;
extern std::unordered_map<std::string, DWORD> pathAttributes;
} // namespace SysCalls

struct ClCacheDefaultConfigWindowsTest : public ::testing::Test {
    ClCacheDefaultConfigWindowsTest()
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
        SysCalls::pathAttributes.clear();
    }

  protected:
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup;
    std::unordered_map<std::string, std::string> mockableEnvs;

    VariableBackup<DWORD> getLastErrorResultBackup;
    VariableBackup<HRESULT> shGetKnownFolderPathResultBackup;
    VariableBackup<size_t> createDirectoryACalledBackup;
    VariableBackup<BOOL> createDirectoryAResultBackup;
};

TEST_F(ClCacheDefaultConfigWindowsTest, GivenAllEnvVarWhenProperlySetThenCorrectConfigIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["NEO_CACHE_DIR"] = "ult\\directory\\";

    DWORD getFileAttributesResultMock = FILE_ATTRIBUTE_DIRECTORY;
    VariableBackup<DWORD> pathExistsMockBackup(&NEO::SysCalls::getFileAttributesResult, getFileAttributesResultMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, "ult\\directory\\");
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenNonExistingPathWhenGetCompilerCacheConfigThenConfigWithDisabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["NEO_CACHE_DIR"] = "ult\\directory\\";

    DWORD getFileAttributesResultMock = INVALID_FILE_ATTRIBUTES;
    VariableBackup<DWORD> pathExistsMockBackup(&NEO::SysCalls::getFileAttributesResult, getFileAttributesResultMock);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenLocalAppDataCachePathSetWhenGetCompilerCacheConfigThenConfigWithEnabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";

    SysCalls::pathAttributes["C:\\Users\\user1\\AppData\\Local\\NEO"] = FILE_ATTRIBUTE_DIRECTORY;
    SysCalls::pathAttributes["C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache"] = FILE_ATTRIBUTE_DIRECTORY;

    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenNeoCacheDirNotSetAndLocalAppDataCachePathNotSetWhenGetCompilerCacheConfigThenConfigWithDisabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";

    SysCalls::shGetKnownFolderPathResult = S_FALSE;

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenLocalAppDataSetAndNonExistingNeoDirectoryWhenGetCompilerCacheConfigThenNeoDirectoryIsCreatedAndConfigWithEnabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";

    SysCalls::pathAttributes["C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache"] = FILE_ATTRIBUTE_DIRECTORY;
    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenLocalAppDataSetAndNonExistingNeoDirectoryWhenGetCompilerCacheConfigAndNeoDirCreationFailsThenConfigWithDisabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";

    SysCalls::pathAttributes["C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache"] = FILE_ATTRIBUTE_DIRECTORY;
    SysCalls::shGetKnownFolderPathResult = S_OK;
    SysCalls::createDirectoryAResult = FALSE;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
    EXPECT_TRUE(cacheConfig.cacheDir.empty());
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenLocalAppDataSetAndNonExistingNeoCompilerCacheDirectoryWhenGetCompilerCacheConfigThenNeoCompilerCacheDirectoryIsCreatedAndConfigWithEnabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";

    SysCalls::pathAttributes["C:\\Users\\user1\\AppData\\Local\\NEO"] = FILE_ATTRIBUTE_DIRECTORY;

    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenLocalAppDataSetAndNonExistingNeoCompilerCacheDirectoryWhenGetCompilerCacheConfigAndDirectoryCreationFailsThenConfigWithDisabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";

    SysCalls::pathAttributes["C:\\Users\\user1\\AppData\\Local\\NEO"] = FILE_ATTRIBUTE_DIRECTORY;

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

TEST_F(ClCacheDefaultConfigWindowsTest, GivenLocalAppDataSetWhenGetCompilerCacheConfigAndNeoCompilerCacheDirectoryAlreadyExistsThenConfigWithEnabledCacheIsReturned) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";

    SysCalls::pathAttributes["C:\\Users\\user1\\AppData\\Local\\NEO"] = FILE_ATTRIBUTE_DIRECTORY;

    SysCalls::shGetKnownFolderPathResult = S_OK;
    SysCalls::createDirectoryAResult = FALSE;
    SysCalls::getLastErrorResult = ERROR_ALREADY_EXISTS;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, 22u);
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
    EXPECT_EQ(1u, SysCalls::createDirectoryACalled);
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenCacheMaxSizeSetTo0WhenGetDefaultConfigThenCacheSizeIsSetToMaxSize) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "0";
    mockableEnvs["NEO_CACHE_DIR"] = "ult\\directory\\";

    SysCalls::pathAttributes["ult\\directory\\"] = FILE_ATTRIBUTE_DIRECTORY;

    auto cacheConfig = getDefaultCompilerCacheConfig();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, std::numeric_limits<size_t>::max());
    EXPECT_EQ(cacheConfig.cacheDir, "ult\\directory\\");
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenCachePathExistsAndNoEnvVarsSetWhenGetDefaultCompilerCacheConfigThenCacheIsEnabled) {
    DWORD getFileAttributesResultMock = FILE_ATTRIBUTE_DIRECTORY;
    VariableBackup<DWORD> pathExistsMockBackup(&NEO::SysCalls::getFileAttributesResult, getFileAttributesResultMock);

    SysCalls::shGetKnownFolderPathResult = S_OK;

    const wchar_t *localAppDataPath = L"C:\\Users\\user1\\AppData\\Local";
    wcscpy(SysCalls::shGetKnownFolderSetPath, localAppDataPath);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    const std::string expectedCacheDirPath = "C:\\Users\\user1\\AppData\\Local\\NEO\\neo_compiler_cache";

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_EQ(cacheConfig.cacheFileExtension, ApiSpecificConfig::compilerCacheFileExtension().c_str());
    EXPECT_EQ(cacheConfig.cacheSize, static_cast<size_t>(MemoryConstants::gigaByte));
    EXPECT_EQ(cacheConfig.cacheDir, expectedCacheDirPath);
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenNeoCachePersistentSetToZeroWhenGetDefaultCompilerCacheConfigThenCacheIsDisabled) {
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "0";

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();

    EXPECT_FALSE(cacheConfig.enabled);
}

TEST_F(ClCacheDefaultConfigWindowsTest, GivenPrintDebugMessagesWhenCacheIsEnabledThenMessageWithPathIsPrintedToStdout) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(true);

    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "22";
    mockableEnvs["NEO_CACHE_DIR"] = "ult\\directory\\";

    DWORD getFileAttributesResultMock = FILE_ATTRIBUTE_DIRECTORY;
    VariableBackup<DWORD> pathExistsMockBackup(&NEO::SysCalls::getFileAttributesResult, getFileAttributesResultMock);

    StreamCapture capture;
    capture.captureStdout();
    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(cacheConfig.enabled);
    EXPECT_STREQ(output.c_str(), "NEO_CACHE_PERSISTENT is enabled. Cache is located in: ult\\directory\\\n\n");
}

} // namespace NEO
