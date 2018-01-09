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

#include "runtime/sku_info/operations/sku_info_receiver.h"
#include "unit_tests/sku_info/sku_info_base_reference.h"
#include "gtest/gtest.h"

using namespace OCLRT;

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
