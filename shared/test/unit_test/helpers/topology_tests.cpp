/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/topology.h"
#include "shared/test/common/test_macros/test.h"

#include <array>

using namespace NEO;

TEST(TopologyTest, givenGeometryAndComputeSlicesWhenGetTopologyInfoThenOnlyComputeSlicesAreProcessedAndInfoIsReturnedAndSliceIndicesAreSet) {
    uint8_t dssGeometry[]{0b1111'1111, 0};
    uint8_t dssCompute[]{0b1111'1111, 0b1111'1111};
    uint8_t l3Banks[]{0b0000'1111};
    uint8_t eu[]{0b0000'1111};

    TopologyBitmap topologyBitmap = {
        .dssGeometry = dssGeometry,
        .dssCompute = dssCompute,
        .l3Banks = l3Banks,
        .eu = eu,
    };

    TopologyLimits topologyLimits = {
        .maxSlices = 2,
        .maxSubSlicesPerSlice = 8,
        .maxEusPerSubSlice = 4,
    };

    TopologyMapping topologyMapping{};

    auto topologyInfo = getTopologyInfo(topologyBitmap, topologyLimits, topologyMapping);

    EXPECT_EQ(topologyInfo.sliceCount, 2);
    EXPECT_EQ(topologyInfo.subSliceCount, 16);
    EXPECT_EQ(topologyInfo.euCount, 64);
    EXPECT_EQ(topologyInfo.l3BankCount, 4);

    std::array<int, 2> expectedSliceIndices{0, 1};

    ASSERT_EQ(topologyMapping.sliceIndices.size(), expectedSliceIndices.size());

    for (auto i = 0; i < std::ssize(topologyMapping.sliceIndices); ++i) {
        EXPECT_EQ(topologyMapping.sliceIndices[i], expectedSliceIndices[i]);
    }

    EXPECT_EQ(topologyMapping.subsliceIndices.size(), 0u);
}

TEST(TopologyTest, givenGeometrySlicesAndNoComputeSlicesWhenGetTopologyInfoThenInfoIsReturnedAndSliceIndicesAreSet) {
    uint8_t dssGeometry[]{0b1011'1111, 0b1111'1101};
    uint8_t l3Banks[]{0b0000'1101};
    uint8_t eu[]{0b0000'1011};

    TopologyBitmap topologyBitmap = {
        .dssGeometry = dssGeometry,
        .l3Banks = l3Banks,
        .eu = eu,
    };

    TopologyLimits topologyLimits = {
        .maxSlices = 2,
        .maxSubSlicesPerSlice = 8,
        .maxEusPerSubSlice = 4,
    };

    TopologyMapping topologyMapping{};

    auto topologyInfo = getTopologyInfo(topologyBitmap, topologyLimits, topologyMapping);

    EXPECT_EQ(topologyInfo.sliceCount, 2);
    EXPECT_EQ(topologyInfo.subSliceCount, 14);
    EXPECT_EQ(topologyInfo.euCount, 42);
    EXPECT_EQ(topologyInfo.l3BankCount, 3);

    std::array<int, 2> expectedSliceIndices{0, 1};

    ASSERT_EQ(topologyMapping.sliceIndices.size(), expectedSliceIndices.size());

    for (auto i = 0; i < std::ssize(topologyMapping.sliceIndices); ++i) {
        EXPECT_EQ(topologyMapping.sliceIndices[i], expectedSliceIndices[i]);
    }

    EXPECT_EQ(topologyMapping.subsliceIndices.size(), 0u);
}

TEST(TopologyTest, givenSingleSliceEnabledWhenGetTopologyInfoThenInfoIsReturnedAndSliceIndicesAndSubSliceIndicesAreSet) {
    uint8_t dssCompute[]{0, 0b1010'1010};
    uint8_t l3Banks[]{0b0000'1111};
    uint8_t eu[]{0b0000'1111};

    TopologyBitmap topologyBitmap = {
        .dssCompute = dssCompute,
        .l3Banks = l3Banks,
        .eu = eu,
    };

    TopologyLimits topologyLimits = {
        .maxSlices = 2,
        .maxSubSlicesPerSlice = 8,
        .maxEusPerSubSlice = 4,
    };

    TopologyMapping topologyMapping{};

    auto topologyInfo = getTopologyInfo(topologyBitmap, topologyLimits, topologyMapping);

    EXPECT_EQ(topologyInfo.sliceCount, 1);
    EXPECT_EQ(topologyInfo.subSliceCount, 4);
    EXPECT_EQ(topologyInfo.euCount, 16);
    EXPECT_EQ(topologyInfo.l3BankCount, 4);

    std::array<int, 1> expectedSliceIndices{1};

    ASSERT_EQ(topologyMapping.sliceIndices.size(), expectedSliceIndices.size());
    EXPECT_EQ(topologyMapping.sliceIndices[0], expectedSliceIndices[0]);

    std::array<int, 4> expectedSubSliceIndices{1, 3, 5, 7};

    ASSERT_EQ(topologyMapping.subsliceIndices.size(), expectedSubSliceIndices.size());
    for (auto i = 0; i < std::ssize(topologyMapping.subsliceIndices); ++i) {
        EXPECT_EQ(topologyMapping.subsliceIndices[i], expectedSubSliceIndices[i]);
    }
}

TEST(TopologyTest, givenTilesWithTheSameTopologyWhenGetTopologyInfoMultiTileThenCommonInfoIsReturnedAndIndicesAreSetRespectively) {
    uint8_t dssCompute[]{0b0111'1111, 0b1111'1111};
    uint8_t l3Banks[]{0b0000'1111};
    uint8_t eu[]{0b0000'1111};

    TopologyBitmap topologyBitmap = {
        .dssCompute = dssCompute,
        .l3Banks = l3Banks,
        .eu = eu,
    };

    std::array<TopologyBitmap, 2> topologyBitmaps{topologyBitmap, topologyBitmap};

    TopologyLimits topologyLimits = {
        .maxSlices = 2,
        .maxSubSlicesPerSlice = 8,
        .maxEusPerSubSlice = 4,
    };

    TopologyMap topologyMap{};

    auto topologyInfo = getTopologyInfoMultiTile(topologyBitmaps, topologyLimits, topologyMap);

    EXPECT_EQ(topologyInfo.sliceCount, 2);
    EXPECT_EQ(topologyInfo.subSliceCount, 15);
    EXPECT_EQ(topologyInfo.euCount, 60);
    EXPECT_EQ(topologyInfo.l3BankCount, 4);

    for (auto tileId = 0; tileId < std::ssize(topologyBitmaps); ++tileId) {
        std::array<int, 2> expectedSliceIndices{0, 1};

        ASSERT_EQ(topologyMap[tileId].sliceIndices.size(), expectedSliceIndices.size());

        for (auto i = 0; i < std::ssize(topologyMap[tileId].sliceIndices); ++i) {
            EXPECT_EQ(topologyMap[tileId].sliceIndices[i], expectedSliceIndices[i]);
        }

        EXPECT_EQ(topologyMap[tileId].subsliceIndices.size(), 0u);
    }
}

TEST(TopologyTest, givenTilesWithDifferentTopologyCountsWhenGetTopologyInfoMultiTileThenCommonInfoIsReturnedAndIndicesAreSetRespectively) {
    uint8_t dssComputeT0[]{0b1111'1111, 0b1111'1111};
    uint8_t l3BanksT0[]{0b0000'1111};
    uint8_t euT0[]{0b0011'1111};

    TopologyBitmap topologyBitmapT0 = {
        .dssCompute = dssComputeT0,
        .l3Banks = l3BanksT0,
        .eu = euT0,
    };

    uint8_t dssComputeT1[]{0b0000'0000, 0b1111'1111};
    uint8_t l3BanksT1[]{0b0000'0011};
    uint8_t euT1[]{0b0000'1111};

    TopologyBitmap topologyBitmapT1 = {
        .dssCompute = dssComputeT1,
        .l3Banks = l3BanksT1,
        .eu = euT1,
    };

    std::array<TopologyBitmap, 2> topologyBitmaps{topologyBitmapT0, topologyBitmapT1};

    TopologyLimits topologyLimits = {
        .maxSlices = 2,
        .maxSubSlicesPerSlice = 8,
        .maxEusPerSubSlice = 4,
    };

    TopologyMap topologyMap{};

    auto topologyInfo = getTopologyInfoMultiTile(topologyBitmaps, topologyLimits, topologyMap);

    EXPECT_EQ(topologyInfo.sliceCount, 1);
    EXPECT_EQ(topologyInfo.subSliceCount, 8);
    EXPECT_EQ(topologyInfo.euCount, 32);
    EXPECT_EQ(topologyInfo.l3BankCount, 2);

    // Tile 0

    std::array<int, 2> expectedSliceIndicesT0{0, 1};

    ASSERT_EQ(topologyMap[0].sliceIndices.size(), expectedSliceIndicesT0.size());

    for (auto i = 0; i < std::ssize(topologyMap[0].sliceIndices); ++i) {
        EXPECT_EQ(topologyMap[0].sliceIndices[i], expectedSliceIndicesT0[i]);
    }

    EXPECT_EQ(topologyMap[0].subsliceIndices.size(), 0u);

    // Tile 1

    std::array<int, 1> expectedSliceIndicesT1{1};

    ASSERT_EQ(topologyMap[1].sliceIndices.size(), expectedSliceIndicesT1.size());
    EXPECT_EQ(topologyMap[1].sliceIndices[0], expectedSliceIndicesT1[0]);

    std::array<int, 8> expectedSubSliceIndicesT1{0, 1, 2, 3, 4, 5, 6, 7};

    for (auto i = 0; i < std::ssize(topologyMap[1].subsliceIndices); ++i) {
        EXPECT_EQ(topologyMap[1].subsliceIndices[i], expectedSubSliceIndicesT1[i]);
    }
}

TEST(TopologyTest, givenNoTilesWhenGetTopologyInfoMultiTileThenEmptyInfoIsReturned) {
    std::array<TopologyBitmap, 0> topologyBitmap;

    TopologyLimits topologyLimits = {
        .maxSlices = 2,
        .maxSubSlicesPerSlice = 8,
        .maxEusPerSubSlice = 4,
    };

    TopologyMap topologyMap{};

    auto topologyInfo = getTopologyInfoMultiTile(topologyBitmap, topologyLimits, topologyMap);

    EXPECT_EQ(topologyInfo.sliceCount, 0);
    EXPECT_EQ(topologyInfo.subSliceCount, 0);
    EXPECT_EQ(topologyInfo.euCount, 0);
    EXPECT_EQ(topologyInfo.l3BankCount, 0);
    EXPECT_EQ(topologyMap.size(), 0u);
}
