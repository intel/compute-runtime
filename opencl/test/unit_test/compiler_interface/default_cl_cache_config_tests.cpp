/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/compiler_interface/default_cl_cache_config.h"

TEST(CompilerCache, GivenDefaultClCacheConfigThenValuesAreProperlyPopulated) {
    auto cacheConfig = NEO::getDefaultClCompilerCacheConfig();
    EXPECT_STREQ("cl_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".cl_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_TRUE(cacheConfig.enabled);
}

TEST(CompilerCacheTests, GivenExistingConfigWhenLoadingFromCacheThenBinaryIsLoaded) {
    NEO::CompilerCache cache(NEO::getDefaultClCompilerCacheConfig());
    static const char *hash = "SOME_HASH";
    std::unique_ptr<char> data(new char[32]);
    for (size_t i = 0; i < 32; i++)
        data.get()[i] = static_cast<char>(i);

    bool ret = cache.cacheBinary(hash, static_cast<const char *>(data.get()), 32);
    EXPECT_TRUE(ret);

    size_t size;
    auto loadedBin = cache.loadCachedBinary(hash, size);
    EXPECT_NE(nullptr, loadedBin);
    EXPECT_NE(0U, size);
}