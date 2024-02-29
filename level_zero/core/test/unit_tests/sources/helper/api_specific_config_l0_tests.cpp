/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "level_zero/core/source/compiler_interface/l0_reg_path.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ApiSpecificConfigL0Tests, WhenGettingApiTypeThenCorrectTypeIsReturned) {
    EXPECT_EQ(ApiSpecificConfig::L0, ApiSpecificConfig::getApiType());
}

TEST(ApiSpecificConfigL0Tests, WhenGettingAUBPrefixByApiTypeL0IsReturned) {
    EXPECT_EQ(0, strcmp("l0_", ApiSpecificConfig::getAubPrefixForSpecificApi().c_str()));
}

TEST(ApiSpecificConfigL0Tests, WhenGettingNameL0IsReturned) {
    EXPECT_EQ(0, strcmp("l0", ApiSpecificConfig::getName().c_str()));
}

TEST(ApiSpecificConfigL0Tests, WhenCheckingIfStatelessCompressionIsSupportedThenReturnFalse) {
    EXPECT_FALSE(ApiSpecificConfig::isStatelessCompressionSupported());
}

TEST(ApiSpecificConfigL0Tests, givenMaxAllocSizeWhenGettingReducedMaxAllocSizeThenReturnSameValue) {
    EXPECT_EQ(1024u, ApiSpecificConfig::getReducedMaxAllocSize(1024));
}

TEST(ApiSpecificConfigL0Tests, WhenGettingRegistryPathThenL0RegistryPathIsReturned) {
    EXPECT_STREQ(L0::registryPath, ApiSpecificConfig::getRegistryPath());
}

TEST(ApiSpecificConfigL0Tests, WhenCheckingIfDeviceAllocationCacheIsEnabledThenReturnFalse) {
    EXPECT_FALSE(ApiSpecificConfig::isDeviceAllocationCacheEnabled());
}

TEST(ApiSpecificConfigL0Tests, GivenDebugFlagCombinationsGetCorrectSharedAllocPrefetchEnabled) {
    DebugManagerStateRestore restore;

    EXPECT_FALSE(ApiSpecificConfig::isSharedAllocPrefetchEnabled());

    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(true);

    EXPECT_TRUE(ApiSpecificConfig::isSharedAllocPrefetchEnabled());

    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(false);

    debugManager.flags.EnableBOChunkingPrefetch.set(true);

    EXPECT_FALSE(ApiSpecificConfig::isSharedAllocPrefetchEnabled());

    debugManager.flags.EnableBOChunking.set(1);

    EXPECT_TRUE(ApiSpecificConfig::isSharedAllocPrefetchEnabled());

    debugManager.flags.EnableBOChunking.set(2);

    EXPECT_FALSE(ApiSpecificConfig::isSharedAllocPrefetchEnabled());

    debugManager.flags.EnableBOChunking.set(3);

    EXPECT_TRUE(ApiSpecificConfig::isSharedAllocPrefetchEnabled());

    debugManager.flags.EnableBOChunking.set(0);

    EXPECT_FALSE(ApiSpecificConfig::isSharedAllocPrefetchEnabled());
}

TEST(ImplicitScalingApiTests, givenLevelZeroApiUsedThenSupportEnabled) {
    EXPECT_TRUE(ImplicitScaling::apiSupport);
}

TEST(ApiSpecificConfigL0Tests, WhenGettingCompilerCacheDirThenReturnProperDirString) {
    EXPECT_EQ(0, strcmp("l0_cache_dir", ApiSpecificConfig::compilerCacheDir().c_str()));
}

TEST(ApiSpecificConfigL0Tests, WhenGettingCompilerCacheLocationThenReturnProperLocationString) {
    EXPECT_EQ(0, strcmp("l0_cache", ApiSpecificConfig::compilerCacheLocation().c_str()));
}

TEST(ApiSpecificConfigL0Tests, WhenGettingCompilerCacheFileExtensionThenReturnProperFileExtensionString) {
    EXPECT_EQ(0, strcmp(".l0_cache", ApiSpecificConfig::compilerCacheFileExtension().c_str()));
}

TEST(ApiSpecificConfigL0Tests, WhenCheckingIfCompilerCacheIsEnabledByDefaultThenReturnTrue) {
    EXPECT_EQ(0l, ApiSpecificConfig::compilerCacheDefaultEnabled());
}

} // namespace NEO
