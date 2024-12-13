/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using LnlProductHelperWindows = ProductHelperTest;

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenDcFlushMitigationThenReturnFalse) {
    EXPECT_FALSE(productHelper->mitigateDcFlush());
    EXPECT_FALSE(productHelper->isDcFlushMitigated());
}

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenOverridePatIndexCalledThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowDcFlush.set(1);

    uint64_t expectedPatIndex = 6u;
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::bufferHostMemory));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::mapAllocation));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmCpu));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmZeroCopy));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHostMemory));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::timestampPacketTagBuffer));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::tagBuffer));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::linearStream));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHeap));

    debugManager.flags.AllowDcFlush.set(0);

    uint64_t expectedPatIndexOverride = 2u;
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::bufferHostMemory));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::mapAllocation));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmCpu));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmZeroCopy));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHostMemory));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::timestampPacketTagBuffer));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::tagBuffer));

    expectedPatIndexOverride = 1u;
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::linearStream));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHeap));

    expectedPatIndexOverride = 19u;
    debugManager.flags.OverrideReadWritePatForDcFlushMitigation.set(static_cast<int32_t>(expectedPatIndexOverride));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::bufferHostMemory));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::mapAllocation));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmCpu));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmZeroCopy));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHostMemory));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::timestampPacketTagBuffer));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::tagBuffer));

    expectedPatIndexOverride = 33u;
    debugManager.flags.OverrideWriteOnlyPatForDcFlushMitigation.set(static_cast<int32_t>(expectedPatIndexOverride));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::linearStream));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHeap));
}

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenCheckIsCopyBufferRectSplitSupportedThenReturnsTrue) {
    EXPECT_TRUE(productHelper->isCopyBufferRectSplitSupported());
}