/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/sku_info/operations/windows/sku_info_receiver.h"

#include "opencl/test/unit_test/sku_info/sku_info_base_reference.h"

#include "gtest/gtest.h"

using namespace NEO;

inline bool operator==(const FeatureTable &lhs, const FeatureTable &rhs) {
    return lhs.ftrBcsInfo == rhs.ftrBcsInfo && lhs.packed == rhs.packed;
}

TEST(SkuInfoReceiverTest, givenAdapterInfoWhenReceivingThenUpdateFtrTable) {
    FeatureTable refFeatureTable = {};
    FeatureTable requestedFeatureTable = {};
    ADAPTER_INFO adapterInfo = {};
    memset(&adapterInfo.SkuTable, ~0, sizeof(adapterInfo.SkuTable));

    EXPECT_EQ(1lu, requestedFeatureTable.ftrBcsInfo.to_ulong());

    SkuInfoReceiver::receiveFtrTableFromAdapterInfo(&requestedFeatureTable, &adapterInfo);

    SkuInfoBaseReference::fillReferenceFtrToReceive(refFeatureTable);

    EXPECT_EQ(1lu, requestedFeatureTable.ftrBcsInfo.to_ulong());

    EXPECT_TRUE(refFeatureTable == requestedFeatureTable);

    refFeatureTable.flags.ftr3dMidBatchPreempt = false;
    requestedFeatureTable.flags.ftr3dMidBatchPreempt = true;

    EXPECT_FALSE(refFeatureTable == requestedFeatureTable);
}

TEST(SkuInfoReceiverTest, givenFeatureTableWhenDifferentDataThenEqualityOperatorReturnsCorrectScore) {
    FeatureTable refFeatureTable = {};
    FeatureTable requestedFeatureTable = {};

    refFeatureTable.ftrBcsInfo = 1;
    requestedFeatureTable.ftrBcsInfo = 0;

    EXPECT_FALSE(refFeatureTable == requestedFeatureTable);

    refFeatureTable.ftrBcsInfo = 0;
    requestedFeatureTable.ftrBcsInfo = 1;

    EXPECT_FALSE(refFeatureTable == requestedFeatureTable);

    refFeatureTable.ftrBcsInfo = 1;
    requestedFeatureTable.ftrBcsInfo = 1;
    refFeatureTable.packed[0] = 1u;
    requestedFeatureTable.packed[0] = 0;

    EXPECT_FALSE(refFeatureTable == requestedFeatureTable);

    refFeatureTable.packed[0] = 0;
    requestedFeatureTable.packed[0] = 1;

    EXPECT_FALSE(refFeatureTable == requestedFeatureTable);

    refFeatureTable.packed[0] = 0;
    requestedFeatureTable.packed[0] = 0;
    refFeatureTable.packed[1] = 0;
    requestedFeatureTable.packed[1] = 1;

    EXPECT_FALSE(refFeatureTable == requestedFeatureTable);

    refFeatureTable.packed[0] = 0;
    requestedFeatureTable.packed[0] = 0;
    refFeatureTable.packed[1] = 1;
    requestedFeatureTable.packed[1] = 0;

    EXPECT_FALSE(refFeatureTable == requestedFeatureTable);

    refFeatureTable.packed[1] = 1;
    requestedFeatureTable.packed[1] = 1;

    EXPECT_TRUE(refFeatureTable == requestedFeatureTable);
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
