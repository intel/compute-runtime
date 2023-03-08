/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/test/common/test_macros/test.h"

TEST(CompilerCache, GivenDefaultClCacheConfigThenValuesAreProperlyPopulated) {
    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("cl_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".cl_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_TRUE(cacheConfig.enabled);
}
