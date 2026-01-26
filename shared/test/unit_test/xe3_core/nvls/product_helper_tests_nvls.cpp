/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "neo_aot_platforms.h"

using namespace NEO;

using NvlsProductHelper = ProductHelperTest;

NVLSTEST_F(NvlsProductHelper, whenGettingPreferredAllocationMethodThenAllocateByKmdIsReturned) {
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
    }
}

NVLSTEST_F(NvlsProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::NVL_S_A0);
}

HWTEST_EXCLUDE_PRODUCT(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned, IGFX_NVL_XE3G);
NVLSTEST_F(NvlsProductHelper, givenCompilerProductHelperWhenGetMidThreadPreemptionSupportThenCorrectValueIsSet) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrWalkerMTP = false;
    EXPECT_FALSE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    hwInfo.featureTable.flags.ftrWalkerMTP = true;
    EXPECT_TRUE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
}

NVLSTEST_F(NvlsProductHelper, givenProductHelperWhenCheckoverrideAllocationCpuCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCpuCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCpuCacheable(allocationData));
}

NVLSTEST_F(NvlsProductHelper, givenProductHelperWhenCallIsStagingBuffersEnabledThenReturnTrue) {
    EXPECT_TRUE(productHelper->isStagingBuffersEnabled());
}

NVLSTEST_F(NvlsProductHelper, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

NVLSTEST_F(NvlsProductHelper, givenProductHelperWhenCheckingInitializeInternalEngineImmediatelyThenCorrectValueIsReturned) {
    EXPECT_FALSE(productHelper->initializeInternalEngineImmediately());
}
NVLSTEST_F(NvlsProductHelper, givenProductHelperWhenIsMisalignedUserPtr2WayCoherentThenReturnTrue) {
    EXPECT_TRUE(productHelper->isMisalignedUserPtr2WayCoherent());
}