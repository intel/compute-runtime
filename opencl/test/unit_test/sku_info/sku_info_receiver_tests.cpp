/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/sku_info/operations/windows/sku_info_receiver.h"
#include "opencl/test/unit_test/sku_info/sku_info_base_reference.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(SkuInfoReceiverTest, givenAdapterInfoWhenReceivingThenUpdateFtrTable) {
    FeatureTable refFeatureTable = {};
    FeatureTable requestedFeatureTable = {};
    ADAPTER_INFO adapterInfo = {};
    memset(&adapterInfo.SkuTable, ~0, sizeof(adapterInfo.SkuTable));
    SkuInfoReceiver::receiveFtrTableFromAdapterInfo(&requestedFeatureTable, &adapterInfo);

    SkuInfoBaseReference::fillReferenceFtrToReceive(refFeatureTable);

    EXPECT_TRUE(memcmp(&requestedFeatureTable, &refFeatureTable, sizeof(FeatureTable)) == 0);
}

TEST(SkuInfoReceiverTest, givenAdapterInfoWhenReceivingThenUpdateWaTable) {
    WorkaroundTable refWaTable = {};
    WorkaroundTable requestedWaTable = {};
    ADAPTER_INFO adapterInfo = {};
    memset(&adapterInfo.WaTable, ~0, sizeof(adapterInfo.WaTable));
    SkuInfoReceiver::receiveWaTableFromAdapterInfo(&requestedWaTable, &adapterInfo);

    SkuInfoBaseReference::fillReferenceWaToReceive(refWaTable);

    EXPECT_TRUE(memcmp(&requestedWaTable, &refWaTable, sizeof(WorkaroundTable)) == 0);
}
