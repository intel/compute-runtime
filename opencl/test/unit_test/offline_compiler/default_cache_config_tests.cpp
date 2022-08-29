/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/test/common/test_macros/test.h"

TEST(CompilerCache, GivenDefaultCacheConfigThenValuesAreProperlyPopulated) {
    auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
    EXPECT_STREQ("ocloc_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".ocloc_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_TRUE(cacheConfig.enabled);
}
