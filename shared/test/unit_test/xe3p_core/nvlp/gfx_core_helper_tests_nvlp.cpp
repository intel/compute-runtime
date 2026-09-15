/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "neo_aot_platforms.h"
#include "per_product_test_definitions.h"

using GfxCoreHelperTestsNvlp = GfxCoreHelperTest;

NVLPTEST_F(GfxCoreHelperTestsNvlp, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::commandBuffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

NVLPTEST_F(GfxCoreHelperTestsNvlp, WhenAskingForDcFlushThenReturnFalse) {
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()));
}

NVLPTEST_F(GfxCoreHelperTestsNvlp, whenGetL1CachePolicyThenReturnCorrectValue) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.getL1CachePolicy(false), FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB);
    EXPECT_EQ(productHelper.getL1CachePolicy(true), FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP);
}

NVLPTEST_F(GfxCoreHelperTestsNvlp, givenProductHelperWhenCallIsStagingBuffersEnabledThenReturnTrue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isStagingBuffersEnabled());
}

NVLPTEST_F(GfxCoreHelperTestsNvlp, givenLargeGrfCountWhenCalculateNumThreadsPerThreadGroupThenAdjustOnlyForPlatformsSupportingThoseGrfCounts) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    const auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    constexpr uint32_t simd = 32u;
    constexpr uint32_t totalWorkItems = 2048u;

    rootDeviceEnvironment.getMutableHardwareInfo()->ipVersion.value = AOT::NVL_P_B0;
    EXPECT_EQ(16u, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, totalWorkItems, 448u, rootDeviceEnvironment));
    EXPECT_EQ(24u, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, totalWorkItems, 320u, rootDeviceEnvironment));

    rootDeviceEnvironment.getMutableHardwareInfo()->ipVersion.value = AOT::NVL_P_A0;
    EXPECT_EQ(32u, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, totalWorkItems, 448u, rootDeviceEnvironment));
    EXPECT_EQ(32u, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, totalWorkItems, 320u, rootDeviceEnvironment));
}
