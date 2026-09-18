/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/sku_info/operations/sku_info_transfer.h"
#include "shared/test/unit_test/sku_info/sku_info_base_reference.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(SkuInfoTransferTest, givenFeatureTableWhenFillingStructureForGmmThenCopyOnlySelectedValues) {
    _SKU_FEATURE_TABLE requestedFtrTable = {};
    _SKU_FEATURE_TABLE refFtrTable = {};
    FeatureTable featureTable;

    for (auto &e : featureTable.packed) {
        e = std::numeric_limits<uint32_t>::max();
    }

    SkuInfoTransfer::transferFtrTableForGmm(&requestedFtrTable, &featureTable);

    SkuInfoBaseReference::fillReferenceFtrForTransfer(refFtrTable);

    EXPECT_TRUE(memcmp(&requestedFtrTable, &refFtrTable, sizeof(_SKU_FEATURE_TABLE)) == 0);
}

TEST(SkuInfoTransferTest, givenAppTransientCachingSupportWhenFillingStructureForGmmThenFlagIsCopied) {
    FeatureTable featureTable;
    EXPECT_FALSE(featureTable.flags.ftrAppTransientCaching);

    for (const bool enabled : {false, true}) {
        featureTable.flags.ftrAppTransientCaching = enabled;
        _SKU_FEATURE_TABLE requestedFtrTable = {};
        requestedFtrTable.FtrAppTransientCaching = !enabled;

        SkuInfoTransfer::transferFtrTableForGmm(&requestedFtrTable, &featureTable);

        EXPECT_EQ(enabled, requestedFtrTable.FtrAppTransientCaching);
    }
}

TEST(SkuInfoTransferTest, givenWaTableWhenFillingStructureForGmmThenCopyOnlySelectedValues) {
    _WA_TABLE requestedWaTable = {};
    _WA_TABLE refWaTable = {};
    WorkaroundTable waTable;
    refWaTable = {};

    for (auto &e : waTable.packed) {
        e = std::numeric_limits<uint32_t>::max();
    }

    SkuInfoTransfer::transferWaTableForGmm(&requestedWaTable, &waTable);

    SkuInfoBaseReference::fillReferenceWaForTransfer(refWaTable);

    EXPECT_TRUE(memcmp(&requestedWaTable, &refWaTable, sizeof(_WA_TABLE)) == 0);
}
