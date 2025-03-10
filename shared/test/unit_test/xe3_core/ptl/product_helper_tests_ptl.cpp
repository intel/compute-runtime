/*
 * Copyright (C) 2025 Intel Corporation
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
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "platforms.h"

using namespace NEO;

using PtlProductHelper = ProductHelperTest;

PTLTEST_F(PtlProductHelper, whenGettingPreferredAllocationMethodThenAllocateByKmdIsReturned) {
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
    }
}

PTLTEST_F(PtlProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::PTL_H_A0);
}

HWTEST_EXCLUDE_PRODUCT(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned, IGFX_PTL);
PTLTEST_F(PtlProductHelper, givenCompilerProductHelperWhenGetMidThreadPreemptionSupportThenCorrectValueIsSet) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrWalkerMTP = false;
    EXPECT_FALSE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    hwInfo.featureTable.flags.ftrWalkerMTP = true;
    EXPECT_TRUE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCheckOverrideAllocationCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCacheable(allocationData));
}

PTLTEST_F(PtlProductHelper, givenExternalHostPtrWhenMitigateDcFlushThenOverrideCacheable) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowDcFlush.set(1);

    AllocationData allocationData{};
    allocationData.type = AllocationType::externalHostPtr;
    EXPECT_FALSE(productHelper->overrideAllocationCacheable(allocationData));

    debugManager.flags.AllowDcFlush.set(0);

    for (auto i = 0; i < static_cast<int>(AllocationType::count); ++i) {
        auto allocationType = static_cast<AllocationType>(i);
        allocationData.type = allocationType;
        switch (allocationData.type) {
        case AllocationType::commandBuffer:
            EXPECT_TRUE(productHelper->overrideAllocationCacheable(allocationData));
            break;
        case AllocationType::externalHostPtr:
        case AllocationType::bufferHostMemory:
        case AllocationType::mapAllocation:
        case AllocationType::svmCpu:
        case AllocationType::svmZeroCopy:
        case AllocationType::internalHostMemory:
        case AllocationType::printfSurface:
            EXPECT_TRUE(productHelper->overrideAllocationCacheable(allocationData));
            EXPECT_TRUE(productHelper->overrideCacheableForDcFlushMitigation(allocationData.type));
            break;
        default:
            EXPECT_FALSE(productHelper->overrideAllocationCacheable(allocationData));
            EXPECT_FALSE(productHelper->overrideCacheableForDcFlushMitigation(allocationData.type));
            break;
        }
    }
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCallIsCachingOnCpuAvailableThenFalseIsReturned) {
    EXPECT_FALSE(productHelper->isCachingOnCpuAvailable());
}
