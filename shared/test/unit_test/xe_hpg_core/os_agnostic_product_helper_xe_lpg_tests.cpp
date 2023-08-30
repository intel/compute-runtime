/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/xe_hpg_core/os_agnostic_product_helper_xe_lpg_tests.h"

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
void XeLpgTests::testOverridePatIndex(const ProductHelper &productHelper) {
    uint64_t patIndex = 1u;
    bool isUncached = true;
    EXPECT_EQ(2u, productHelper.overridePatIndex(isUncached, patIndex));

    isUncached = false;
    EXPECT_EQ(patIndex, productHelper.overridePatIndex(isUncached, patIndex));
}

void XeLpgTests::testPreferredAllocationMethod(const ProductHelper &productHelper) {
    for (auto i = 0; i < static_cast<int>(AllocationType::COUNT); i++) {
        auto preferredAllocationMethod = productHelper.getPreferredAllocationMethod(static_cast<AllocationType>(i));
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::AllocateByKmd, preferredAllocationMethod.value());
    }
}
