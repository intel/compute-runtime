/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/sku_info/operations/sku_info_transfer.h"

#include "opencl/test/unit_test/sku_info/sku_info_base_reference.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(SkuInfoTransferTest, givenFeatureTableWhenFillingStructureForGmmThenCopyOnlySelectedValues) {
    _SKU_FEATURE_TABLE requestedFtrTable = {};
    _SKU_FEATURE_TABLE refFtrTable = {};
    FeatureTable featureTable;

    featureTable.packed[0] = 0xFFFFFFFFFFFFFFFF;
    featureTable.packed[1] = 0xFFFFFFFFFFFFFFFF;
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
