/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/sku_info/operations/windows/sku_info_receiver.h"

namespace NEO {
void SkuInfoReceiver::receiveFtrTableFromAdapterInfo(FeatureTable *ftrTable, ADAPTER_INFO_KMD *adapterInfo) {
    receiveFtrTableFromAdapterInfoBase(ftrTable, adapterInfo);
}

void SkuInfoReceiver::receiveWaTableFromAdapterInfo(WorkaroundTable *workaroundTable, ADAPTER_INFO_KMD *adapterInfo) {
    receiveWaTableFromAdapterInfoBase(workaroundTable, adapterInfo);
}
} // namespace NEO
