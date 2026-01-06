/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/os_interface/ocl_reg_path.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ApiSpecificConfigOclTests, WhenGettingApiTypeThenCorrectTypeIsReturned) {
    EXPECT_EQ(ApiSpecificConfig::OCL, ApiSpecificConfig::getApiType());
}

TEST(ApiSpecificConfigOclTests, WhenGettingAUBPrefixByApiTypeOCLIsReturned) {
    EXPECT_EQ(0, strcmp("ocl_", ApiSpecificConfig::getAubPrefixForSpecificApi().c_str()));
}

TEST(ApiSpecificConfigOclTests, WhenGettingNameOCLIsReturned) {
    EXPECT_EQ(0, strcmp("ocl", ApiSpecificConfig::getName().c_str()));
}

TEST(ApiSpecificConfigOclTests, WhenCheckingIfStatelessCompressionIsSupportedThenReturnTrue) {
    EXPECT_TRUE(ApiSpecificConfig::isStatelessCompressionSupported());
}

TEST(ApiSpecificConfigOclTests, givenMaxAllocSizeWhenGettingReducedMaxAllocSizeThenReturnHalfOfThat) {
    EXPECT_EQ(512u, ApiSpecificConfig::getReducedMaxAllocSize(1024));
}

TEST(ApiSpecificConfigOclTests, WhenGettingRegistryPathThenOclRegistryPathIsReturned) {
    EXPECT_STREQ(oclRegPath, ApiSpecificConfig::getRegistryPath());
}

TEST(ApiSpecificConfigOclTests, WhenCheckingIfHostOrDeviceAllocationCacheIsEnabledThenReturnCorrectValue) {
    EXPECT_TRUE(ApiSpecificConfig::isHostAllocationCacheEnabled());
    EXPECT_TRUE(ApiSpecificConfig::isDeviceAllocationCacheEnabled());
}

TEST(ApiSpecificConfigOclTests, givenEnableStatelessCompressionWhenProvidingSvmGpuAllocationThenPreferCompressedBuffer) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    debugManager.flags.EnableStatelessCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::svmGpu,
                                    deviceBitfield);

    EXPECT_TRUE(NEO::CompressionSelector::preferCompressedAllocation(properties));
}

TEST(ApiSpecificConfigOclTests, givenEnableStatelessCompressionWhenProvidingPrintfSurfaceThenPreferCompressedBuffer) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    debugManager.flags.EnableStatelessCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::printfSurface,
                                    deviceBitfield);

    EXPECT_TRUE(NEO::CompressionSelector::preferCompressedAllocation(properties));
}

TEST(ApiSpecificConfigOclTests, WhenGettingCompilerCacheFileExtensionThenReturnProperFileExtensionString) {
    EXPECT_EQ(0, strcmp(".cl_cache", ApiSpecificConfig::compilerCacheFileExtension().c_str()));
}

TEST(ApiSpecificConfigOclTests, WhenCheckingIfCompilerCacheIsEnabledByDefaultThenReturnTrue) {
    EXPECT_EQ(1u, ApiSpecificConfig::compilerCacheDefaultEnabled());
}

TEST(ApiSpecificConfigOclTests, WhenCheckingIsUpdateTagFromWaitEnabledForHeaplessThenTrueIsReturned) {
    EXPECT_TRUE(ApiSpecificConfig::isUpdateTagFromWaitEnabledForHeapless());
}

} // namespace NEO
