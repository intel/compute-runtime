/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
