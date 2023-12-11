/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using namespace NEO;

using MtlProductHelper = ProductHelperTest;

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckOverrideAllocationCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCacheable(allocationData));
}