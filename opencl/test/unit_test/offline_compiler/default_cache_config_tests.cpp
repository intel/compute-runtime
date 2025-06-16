/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

std::unordered_map<std::string, std::string> mockableEnvValues;

char *mockGetenv(const char *name) noexcept {
    if (mockableEnvValues.find(name) != mockableEnvValues.end()) {
        return const_cast<char *>(mockableEnvValues.find(name)->second.c_str());
    }
    return nullptr;
};

using getenvMockFuncPtr = char *(*)(const char *);

TEST(CompilerCache, GivenDefaultCacheConfigThenValuesAreProperlyPopulated) {
    VariableBackup<getenvMockFuncPtr> getenvBkp(reinterpret_cast<getenvMockFuncPtr *>(&NEO::IoFunctions::getenvPtr), &mockGetenv);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("ocloc_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".ocloc_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(0u, cacheConfig.cacheSize);
    EXPECT_FALSE(cacheConfig.enabled);
}

TEST(CompilerCache, GivenEnvVariableWhenDefaultConfigIsCreatedThenValuesAreProperlyPopulated) {
    std::unordered_map<std::string, std::string> mockableEnvs;
    mockableEnvs["NEO_CACHE_PERSISTENT"] = "1";
    mockableEnvs["NEO_CACHE_MAX_SIZE"] = "1024";
    mockableEnvs["NEO_CACHE_DIR"] = "ult/directory/";

    VariableBackup<decltype(mockableEnvValues)> mockableEnvValuesBackup(&mockableEnvValues, mockableEnvs);
    VariableBackup<getenvMockFuncPtr> getenvBkp(reinterpret_cast<getenvMockFuncPtr *>(&NEO::IoFunctions::getenvPtr), &mockGetenv);

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("ult/directory/", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".ocloc_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(1024u, cacheConfig.cacheSize);
    EXPECT_TRUE(cacheConfig.enabled);

    mockableEnvValues["NEO_CACHE_MAX_SIZE"] = "0";
    cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("ult/directory/", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".ocloc_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(std::numeric_limits<size_t>::max(), cacheConfig.cacheSize);
    EXPECT_TRUE(cacheConfig.enabled);

    mockableEnvValues["NEO_CACHE_MAX_SIZE"] = "1048576";
    mockableEnvValues.erase("NEO_CACHE_DIR");
    cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ("", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(0u, cacheConfig.cacheSize);
    EXPECT_FALSE(cacheConfig.enabled);
}
