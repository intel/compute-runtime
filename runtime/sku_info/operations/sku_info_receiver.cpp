/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sku_info/operations/sku_info_receiver.h"

namespace OCLRT {
void SkuInfoReceiver::receiveFtrTableFromAdapterInfo(FeatureTable *ftrTable, _ADAPTER_INFO *adapterInfo) {
    receiveFtrTableFromAdapterInfoBase(ftrTable, adapterInfo);
}

void SkuInfoReceiver::receiveWaTableFromAdapterInfo(WorkaroundTable *waTable, _ADAPTER_INFO *adapterInfo) {
    receiveWaTableFromAdapterInfoBase(waTable, adapterInfo);
}
} // namespace OCLRT
