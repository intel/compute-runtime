/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

TEST(CompilerCache, GivenDefaultCacheConfigThenValuesAreProperlyPopulated) {
    DebugManagerStateRestore restorer;

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("ocloc_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".ocloc_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(0u, cacheConfig.cacheSize);
    EXPECT_FALSE(cacheConfig.enabled);
}

TEST(CompilerCache, GivenEnvVariableWhenDefaultConfigIsCreatedThenValuesAreProperlyPopulated) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnvCachePersistent.set(1);
    NEO::debugManager.flags.EnvCacheMaxSize.set(1024);
    NEO::debugManager.flags.EnvCacheDir.set("ult/directory/");

    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("ult/directory/", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".ocloc_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(1024u, cacheConfig.cacheSize);
    EXPECT_TRUE(cacheConfig.enabled);

    NEO::debugManager.flags.EnvCacheMaxSize.set(0);
    cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("ult/directory/", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".ocloc_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(std::numeric_limits<size_t>::max(), cacheConfig.cacheSize);
    EXPECT_TRUE(cacheConfig.enabled);

    NEO::debugManager.flags.EnvCacheMaxSize.set(1048576);
    NEO::debugManager.flags.EnvCacheDir.set("");
    cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ("", cacheConfig.cacheFileExtension.c_str());
    EXPECT_EQ(0u, cacheConfig.cacheSize);
    EXPECT_FALSE(cacheConfig.enabled);
}
