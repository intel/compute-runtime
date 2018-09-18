/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sku_info/operations/sku_info_transfer.h"

namespace OCLRT {
void SkuInfoTransfer::transferWaTableForGmm(_WA_TABLE *dstWaTable, const OCLRT::WorkaroundTable *srcWaTable) {
    transferWaTableForGmmBase(dstWaTable, srcWaTable);
}

void SkuInfoTransfer::transferFtrTableForGmm(_SKU_FEATURE_TABLE *dstFtrTable, const OCLRT::FeatureTable *srcFtrTable) {
    transferFtrTableForGmmBase(dstFtrTable, srcFtrTable);
}
} // namespace OCLRT
