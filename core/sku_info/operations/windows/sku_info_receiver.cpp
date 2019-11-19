/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/sku_info/operations/windows/sku_info_receiver.h"

namespace NEO {
void SkuInfoReceiver::receiveFtrTableFromAdapterInfo(FeatureTable *ftrTable, _ADAPTER_INFO *adapterInfo) {
    receiveFtrTableFromAdapterInfoBase(ftrTable, adapterInfo);
}

void SkuInfoReceiver::receiveWaTableFromAdapterInfo(WorkaroundTable *workaroundTable, _ADAPTER_INFO *adapterInfo) {
    receiveWaTableFromAdapterInfoBase(workaroundTable, adapterInfo);
}
} // namespace NEO
