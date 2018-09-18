/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sku_info/operations/sku_info_transfer.h"
#include "unit_tests/sku_info/sku_info_base_reference.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(SkuInfoTransferTest, givenFeatureTableWhenFillingStructureForGmmThenCopyOnlySelectedValues) {
    _SKU_FEATURE_TABLE requestedFtrTable = {};
    _SKU_FEATURE_TABLE refFtrTable = {};
    FeatureTable featureTable;
    memset(reinterpret_cast<void *>(&featureTable), 1, sizeof(FeatureTable));
    SkuInfoTransfer::transferFtrTableForGmm(&requestedFtrTable, &featureTable);

    SkuInfoBaseReference::fillReferenceFtrForTransfer(refFtrTable);

    EXPECT_TRUE(memcmp(&requestedFtrTable, &refFtrTable, sizeof(_SKU_FEATURE_TABLE)) == 0);
}

TEST(SkuInfoTransferTest, givenWaTableWhenFillingStructureForGmmThenCopyOnlySelectedValues) {
    _WA_TABLE requestedWaTable = {};
    _WA_TABLE refWaTable = {};
    WorkaroundTable waTable;
    refWaTable = {};
    memset(reinterpret_cast<void *>(&waTable), 1, sizeof(WorkaroundTable));
    SkuInfoTransfer::transferWaTableForGmm(&requestedWaTable, &waTable);

    SkuInfoBaseReference::fillReferenceWaForTransfer(refWaTable);

    EXPECT_TRUE(memcmp(&requestedWaTable, &refWaTable, sizeof(_WA_TABLE)) == 0);
}
