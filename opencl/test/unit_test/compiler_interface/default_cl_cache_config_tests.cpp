/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

namespace SysCalls {
extern bool pathExistsMock;
}

TEST(CompilerCache, GivenDefaultClCacheConfigWithPathExistsThenValuesAreProperlyPopulated) {
    bool pathExistsMock = true;
    VariableBackup<bool> pathExistsMockBackup(&NEO::SysCalls::pathExistsMock, pathExistsMock);
    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("cl_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".cl_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_TRUE(cacheConfig.enabled);
}

TEST(CompilerCache, GivenDefaultClCacheConfigWithNonExistingPathThenValuesAreProperlyPopulated) {
    bool pathExistsMock = false;
    VariableBackup<bool> pathExistsMockBackup(&NEO::SysCalls::pathExistsMock, pathExistsMock);
    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("cl_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".cl_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_FALSE(cacheConfig.enabled);
}
} // namespace NEO
