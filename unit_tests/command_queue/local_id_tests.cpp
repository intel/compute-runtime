/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/command_queue/local_id_gen.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <cstdint>

using namespace OCLRT;

TEST(LocalID, GRFsPerThread_SIMD8) {
    uint32_t simd = 8;
    EXPECT_EQ(1u, getGRFsPerThread(simd));
}

TEST(LocalID, GRFsPerThread_SIMD16) {
    uint32_t simd = 16;
    EXPECT_EQ(1u, getGRFsPerThread(simd));
}

TEST(LocalID, GRFsPerThread_SIMD32) {
    uint32_t simd = 32;
    EXPECT_EQ(2u, getGRFsPerThread(simd));
}

TEST(LocalID, ThreadsPerWorkgroup) {
    size_t lws = 33;
    uint32_t simd = 32;
    EXPECT_EQ(2u, getThreadsPerWG(simd, lws));
}

TEST(LocalID, PerThreadSizeLocalIDs_SIMD8) {
    uint32_t simd = 8;

    // 3 channels (x,y,z) * 1 GRFs per thread (@SIMD8)
    EXPECT_EQ(3 * sizeof(GRF), getPerThreadSizeLocalIDs(simd));
}

TEST(LocalID, PerThreadSizeLocalIDs_SIMD16) {
    uint32_t simd = 16;

    // 3 channels (x,y,z) * 1 GRFs per thread (@SIMD16)
    EXPECT_EQ(3 * sizeof(GRF), getPerThreadSizeLocalIDs(simd));
}

TEST(LocalID, PerThreadSizeLocalIDs_SIMD32) {
    uint32_t simd = 32;

    // 3 channels (x,y,z) * 2 GRFs per thread (@SIMD32)
    EXPECT_EQ(6 * sizeof(GRF), getPerThreadSizeLocalIDs(simd));
}

struct LocalIDFixture : public ::testing::TestWithParam<std::tuple<int, int, int, int>> {
    void SetUp() override {
        simd = std::get<0>(GetParam());
        localWorkSizeX = std::get<1>(GetParam());
        localWorkSizeY = std::get<2>(GetParam());
        localWorkSizeZ = std::get<3>(GetParam());

        localWorkSize = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
        if (localWorkSize > 256) {
            localWorkSizeY = std::min(256 / localWorkSizeX, localWorkSizeY);
            localWorkSizeZ = std::min(256 / (localWorkSizeX * localWorkSizeY), localWorkSizeZ);
            localWorkSize = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
        }

        const auto bufferSize = 32 * 3 * 16 * sizeof(uint16_t);
        buffer = reinterpret_cast<uint16_t *>(alignedMalloc(bufferSize, 32));
        memset(buffer, 0xff, bufferSize);
    }

    void TearDown() override {
        alignedFree(buffer);
    }

    void validateIDWithinLimits(uint32_t simd, uint32_t lwsX, uint32_t lwsY, uint32_t lwsZ) {
        auto idsPerThread = simd;

        // As per BackEnd HLD, SIMD32 has 32 localIDs per channel.  SIMD8/16 has up to 16 localIDs.
        auto skipPerThread = simd == 32 ? 32 : 16;

        auto pBufferX = buffer;
        auto pBufferY = pBufferX + skipPerThread;
        auto pBufferZ = pBufferY + skipPerThread;

        auto numWorkItems = lwsX * lwsY * lwsZ;

        size_t itemIndex = 0;
        while (numWorkItems > 0) {
            EXPECT_LT(pBufferX[itemIndex], lwsX) << simd << " " << lwsX << " " << lwsY << " " << lwsZ;
            EXPECT_LT(pBufferY[itemIndex], lwsY) << simd << " " << lwsX << " " << lwsY << " " << lwsZ;
            EXPECT_LT(pBufferZ[itemIndex], lwsZ) << simd << " " << lwsX << " " << lwsY << " " << lwsZ;
            ++itemIndex;
            if (idsPerThread == itemIndex) {
                pBufferX += skipPerThread * 3;
                pBufferY += skipPerThread * 3;
                pBufferZ += skipPerThread * 3;

                itemIndex = 0;
            }
            --numWorkItems;
        }
    }

    void validateAllWorkItemsCovered(uint32_t simd, uint32_t lwsX, uint32_t lwsY, uint32_t lwsZ) {
        auto idsPerThread = simd;

        // As per BackEnd HLD, SIMD32 has 32 localIDs per channel.  SIMD8/16 has up to 16 localIDs.
        auto skipPerThread = simd == 32 ? 32 : 16;

        auto pBufferX = buffer;
        auto pBufferY = pBufferX + skipPerThread;
        auto pBufferZ = pBufferY + skipPerThread;

        auto numWorkItems = lwsX * lwsY * lwsZ;

        // Initialize local ID hit table
        uint32_t localIDHitTable[8];
        memset(localIDHitTable, 0, sizeof(localIDHitTable));

        size_t itemIndex = 0;
        while (numWorkItems > 0) {
            // Flatten out the IDs
            auto workItem = pBufferX[itemIndex] + pBufferY[itemIndex] * lwsX + pBufferZ[itemIndex] * lwsX * lwsY;
            ASSERT_LT(workItem, 256u);

            // Look up in the hit table
            auto &hitItem = localIDHitTable[workItem / 32];
            auto hitBit = 1 << (workItem % 32);

            // No double-hits
            EXPECT_EQ(0u, hitItem & hitBit);

            // Set that work item as hit
            hitItem |= hitBit;

            ++itemIndex;
            if (idsPerThread == itemIndex) {
                pBufferX += skipPerThread * 3;
                pBufferY += skipPerThread * 3;
                pBufferZ += skipPerThread * 3;

                itemIndex = 0;
            }
            --numWorkItems;
        }

        // All entries in hit table should be in form of n^2 - 1
        for (uint32_t i : localIDHitTable) {
            EXPECT_EQ(0u, i & (i + 1));
        }
    }

    void validateWalkOrder(uint32_t simd, uint32_t localWorkgroupSizeX, uint32_t localWorkgroupSizeY, uint32_t localWorkgroupSizeZ,
                           const std::array<uint8_t, 3> &dimensionsOrder) {
        std::array<uint8_t, 3> walkOrder = {};
        for (uint32_t i = 0; i < 3; ++i) {
            // inverts the walk order mapping (from DIM_ID->ORDER_ID to ORDER_ID->DIM_ID)
            walkOrder[dimensionsOrder[i]] = i;
        }

        auto skipPerThread = simd == 32 ? 32 : 16;

        auto pBufferX = buffer;
        auto pBufferY = pBufferX + skipPerThread;
        auto pBufferZ = pBufferY + skipPerThread;
        decltype(pBufferX) ids[] = {pBufferX, pBufferY, pBufferZ};
        uint32_t sizes[] = {localWorkgroupSizeX, localWorkgroupSizeY, localWorkgroupSizeZ};

        uint32_t flattenedId = 0;
        for (uint32_t id2 = 0; id2 < sizes[walkOrder[2]]; ++id2) {
            for (uint32_t id1 = 0; id1 < sizes[walkOrder[1]]; ++id1) {
                for (uint32_t id0 = 0; id0 < sizes[walkOrder[0]]; ++id0) {
                    uint32_t threadId = flattenedId / simd;
                    uint32_t channelId = flattenedId % simd;
                    uint16_t foundId0 = ids[walkOrder[0]][channelId + threadId * skipPerThread * 3];
                    uint16_t foundId1 = ids[walkOrder[1]][channelId + threadId * skipPerThread * 3];
                    uint16_t foundId2 = ids[walkOrder[2]][channelId + threadId * skipPerThread * 3];
                    if ((id0 != foundId0) || (id1 != foundId1) || (id2 != foundId2)) {
                        EXPECT_EQ(id0, foundId0) << simd << " X @ (" << id0 << ", " << id1 << ", " << id2 << ") - flat " << flattenedId;
                        EXPECT_EQ(id1, foundId1) << simd << " Y @ (" << id0 << ", " << id1 << ", " << id2 << ") - flat " << flattenedId;
                        EXPECT_EQ(id2, foundId2) << simd << " Z @ (" << id0 << ", " << id1 << ", " << id2 << ") - flat " << flattenedId;
                    }
                    ++flattenedId;
                }
            }
        }
    }

    void dumpBuffer(uint32_t simd, uint32_t lwsX, uint32_t lwsY, uint32_t lwsZ) {
        auto workSize = lwsX * lwsY * lwsZ;
        auto threads = (workSize + simd - 1) / simd;

        auto pBuffer = buffer;

        // As per BackEnd HLD, SIMD32 has 32 localIDs per channel.  SIMD8/16 has up to 16 localIDs.
        auto skipPerThread = simd == 32 ? 32 : 16;

        while (threads-- > 0) {
            auto lanes = std::min(workSize, simd);

            for (auto dimension = 0u; dimension < 3u; ++dimension) {
                for (auto lane = 0u; lane < lanes; ++lane) {
                    printf("%04d ", (unsigned int)pBuffer[lane]);
                }
                pBuffer += skipPerThread;
                printf("\n");
            }

            workSize -= simd;
        }
    }

    // Test parameters
    uint32_t localWorkSizeX;
    uint32_t localWorkSizeY;
    uint32_t localWorkSizeZ;
    uint32_t localWorkSize;
    uint32_t simd;

    // Provide support for a max LWS of 256
    // 32 threads @ SIMD8
    // 3 channels (x/y/z)
    // 16 lanes per thread (SIMD8 - only 8 used)
    uint16_t *buffer;
};

TEST_P(LocalIDFixture, checkIDWithinLimits) {
    generateLocalIDs(buffer, simd, std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSizeX), static_cast<uint16_t>(localWorkSizeY), static_cast<uint16_t>(localWorkSizeZ)}},
                     std::array<uint8_t, 3>{{0, 1, 2}}, false);
    validateIDWithinLimits(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
}

TEST_P(LocalIDFixture, checkAllWorkItemsCovered) {
    generateLocalIDs(buffer, simd, std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSizeX), static_cast<uint16_t>(localWorkSizeY), static_cast<uint16_t>(localWorkSizeZ)}},
                     std::array<uint8_t, 3>{{0, 1, 2}}, false);
    validateAllWorkItemsCovered(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
}

TEST_P(LocalIDFixture, WhenWalkOrderIsXyzThenProperLocalIdsAreGenerated) {
    auto dimensionsOrder = std::array<uint8_t, 3>{{0, 1, 2}};
    generateLocalIDs(buffer, simd, std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSizeX), static_cast<uint16_t>(localWorkSizeY), static_cast<uint16_t>(localWorkSizeZ)}},
                     dimensionsOrder, false);
    validateAllWorkItemsCovered(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
    validateWalkOrder(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ, dimensionsOrder);
}

TEST_P(LocalIDFixture, WhenWalkOrderIsYxzThenProperLocalIdsAreGenerated) {
    auto dimensionsOrder = std::array<uint8_t, 3>{{1, 0, 2}};
    generateLocalIDs(buffer, simd, std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSizeX), static_cast<uint16_t>(localWorkSizeY), static_cast<uint16_t>(localWorkSizeZ)}},
                     dimensionsOrder, false);
    validateAllWorkItemsCovered(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
    validateWalkOrder(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ, dimensionsOrder);
}

TEST_P(LocalIDFixture, WhenWalkOrderIsZyxThenProperLocalIdsAreGenerated) {
    auto dimensionsOrder = std::array<uint8_t, 3>{{2, 1, 0}};
    generateLocalIDs(buffer, simd, std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSizeX), static_cast<uint16_t>(localWorkSizeY), static_cast<uint16_t>(localWorkSizeZ)}},
                     dimensionsOrder, false);
    validateAllWorkItemsCovered(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
    validateWalkOrder(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ, dimensionsOrder);
}

TEST_P(LocalIDFixture, sizeCalculationLocalIDs) {
    auto workItems = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
    auto sizeTotalPerThreadData = getThreadsPerWG(simd, workItems) * getPerThreadSizeLocalIDs(simd);

    // Should be multiple of GRFs
    auto sizeGRF = sizeof(GRF);
    EXPECT_EQ(0u, sizeTotalPerThreadData % sizeGRF);

    auto numGRFsPerThread = (simd == 32) ? 2 : 1;
    auto numThreadsExpected = (workItems + simd - 1) / simd;
    auto numGRFsExpected = 3 * numGRFsPerThread * numThreadsExpected;
    EXPECT_EQ(numGRFsExpected * sizeGRF, sizeTotalPerThreadData);
}

struct LocalIdsLayoutForImagesTest : ::testing::TestWithParam<std::tuple<uint16_t, uint16_t, uint16_t>> {
    void SetUp() override {
        simd = std::get<0>(GetParam());
        localWorkSize = {{std::get<1>(GetParam()),
                          std::get<2>(GetParam()),
                          1u}};
        rowWidth = simd == 32u ? 32u : 16u;
        xDelta = simd == 8u ? 2u : 4u;
    }
    void generateLocalIds() {

        auto numGrfs = (localWorkSize.at(0) * localWorkSize.at(1) + (simd - 1)) / simd;
        elemsInBuffer = 3u * simd * numGrfs;
        if (simd == 8u) {
            elemsInBuffer *= 2;
        }
        size = elemsInBuffer * sizeof(uint16_t);
        memory = allocateAlignedMemory(size, 32);
        memset(memory.get(), 0xff, size);
        buffer = reinterpret_cast<uint16_t *>(memory.get());
        EXPECT_TRUE(isCompatibleWithLayoutForImages(localWorkSize, dimensionsOrder, simd));
        generateLocalIDs(buffer, simd, localWorkSize, dimensionsOrder, true);
    }
    void validateGRF() {
        auto numRows = elemsInBuffer / rowWidth;
        auto numGrfs = numRows / 3u;
        for (auto i = 0u; i < numGrfs; i++) {

            // validate X row
            uint16_t baseX = buffer[i * 3 * rowWidth];
            uint16_t currentX = baseX;
            for (int j = 1; j < simd; j++) {
                if (simd == 32u && localWorkSize.at(1) == 4u && j == 16u) {
                    baseX += xDelta;
                    if (baseX == localWorkSize.at(0)) {
                        baseX = 0;
                    }
                }
                currentX = baseX + ((currentX + 1) & (xDelta - 1));
                EXPECT_EQ(buffer[i * 3 * rowWidth + j], currentX);
            }

            // validate Y row
            uint16_t baseY = buffer[i * 3 * rowWidth + rowWidth];
            for (int j = 0; j < simd; j++) {
                uint16_t expectedY = baseY + ((j / xDelta) & 0b111);
                if (expectedY >= localWorkSize.at(1)) {
                    expectedY -= localWorkSize.at(1);
                }
                EXPECT_EQ(buffer[i * 3 * rowWidth + rowWidth + j], expectedY);
            }

            // validate Z row
            for (int j = 0; j < simd; j++) {
                EXPECT_EQ(buffer[i * 3 * rowWidth + 2 * rowWidth + j], 0u);
            }
        }
    }
    uint16_t simd;
    uint8_t rowWidth;
    uint16_t xDelta;
    std::array<uint16_t, 3> localWorkSize;
    std::array<uint8_t, 3> dimensionsOrder = {{0u, 1u, 2u}};
    uint32_t elemsInBuffer;
    uint32_t size;
    std::unique_ptr<void, std::function<decltype(alignedFree)>> memory;
    uint16_t *buffer;
};

TEST(LocalIdsLayoutForImagesTest, givenLocalWorkSizeCompatibleWithLayoutForImagesWithDefaultDimensionsOrderWhenCheckLayoutForImagesCompatibilityThenReturnTrue) {
    std::array<uint16_t, 3> localWorkSize{{4u, 4u, 1u}};
    std::array<uint8_t, 3> dimensionsOrder = {{0u, 1u, 2u}};
    EXPECT_TRUE(isCompatibleWithLayoutForImages(localWorkSize, dimensionsOrder, 16u));
    EXPECT_TRUE(isCompatibleWithLayoutForImages({{4u, 12u, 1u}}, dimensionsOrder, 32u));
}

TEST(LocalIdsLayoutForImagesTest, givenLocalWorkSizeNotCompatibleWithLayoutForImagesWithDefaultDimensionsOrderWhenCheckLayoutForImagesCompatibilityThenReturnFalse) {
    std::array<uint8_t, 3> dimensionsOrder = {{0u, 1u, 2u}};
    EXPECT_FALSE(isCompatibleWithLayoutForImages({{4u, 4u, 2u}}, dimensionsOrder, 8u));
    EXPECT_FALSE(isCompatibleWithLayoutForImages({{2u, 5u, 1u}}, dimensionsOrder, 8u));
    EXPECT_FALSE(isCompatibleWithLayoutForImages({{1u, 4u, 1u}}, dimensionsOrder, 8u));
}
TEST(LocalIdsLayoutForImagesTest, given4x4x1LocalWorkSizeWithNonDefaultDimensionsOrderWhenCheckLayoutForImagesCompatibilityThenReturnFalse) {
    std::array<uint16_t, 3> localWorkSize{{2u, 4u, 1u}};
    EXPECT_FALSE(isCompatibleWithLayoutForImages(localWorkSize, {{0, 2, 1}}, 8u));
    EXPECT_FALSE(isCompatibleWithLayoutForImages(localWorkSize, {{1, 0, 2}}, 8u));
    EXPECT_FALSE(isCompatibleWithLayoutForImages(localWorkSize, {{1, 2, 0}}, 8u));
    EXPECT_FALSE(isCompatibleWithLayoutForImages(localWorkSize, {{2, 0, 1}}, 8u));
    EXPECT_FALSE(isCompatibleWithLayoutForImages(localWorkSize, {{2, 1, 0}}, 8u));
}

using LocalIdsLayoutTest = ::testing::TestWithParam<uint16_t>;

TEST_P(LocalIdsLayoutTest, givenLocalWorkgroupSize4x4x1WhenGenerateLocalIdsThenHasKernelImagesOnlyFlagDoesntMatter) {
    uint16_t simd = GetParam();
    uint8_t rowWidth = simd == 32 ? 32 : 16;
    uint16_t xDelta = simd == 8u ? 2u : 4u;
    std::array<uint16_t, 3> localWorkSize{{xDelta, 4u, 1u}};
    uint16_t totalLocalWorkSize = 4u * xDelta;
    auto dimensionsOrder = std::array<uint8_t, 3>{{0u, 1u, 2u}};

    auto elemsInBuffer = rowWidth * 3u;
    auto size = elemsInBuffer * sizeof(uint16_t);

    auto alignedMemory1 = allocateAlignedMemory(size, 32);
    auto buffer1 = reinterpret_cast<uint16_t *>(alignedMemory1.get());
    memset(buffer1, 0xff, size);

    auto alignedMemory2 = allocateAlignedMemory(size, 32);
    auto buffer2 = reinterpret_cast<uint16_t *>(alignedMemory2.get());
    memset(buffer2, 0xff, size);

    generateLocalIDs(buffer1, simd, localWorkSize, dimensionsOrder, false);
    generateLocalIDs(buffer2, simd, localWorkSize, dimensionsOrder, true);

    for (auto i = 0u; i < elemsInBuffer / rowWidth; i++) {
        for (auto j = 0u; j < rowWidth; j++) {
            if (j < totalLocalWorkSize) {
                auto offset = (i * rowWidth + j) * sizeof(uint16_t);
                auto cmpValue = memcmp(ptrOffset(buffer1, offset), ptrOffset(buffer2, offset), sizeof(uint16_t));
                EXPECT_EQ(0, cmpValue);
            }
        }
    }
}

TEST_P(LocalIdsLayoutForImagesTest, givenLocalWorkgroupSizeCompatibleWithLayoutForImagesWhenGenerateLocalIdsWithKernelWithOnlyImagesThenAppliesLayoutForImages) {
    generateLocalIds();
    validateGRF();
}

#define SIMDParams ::testing::Values(8, 16, 32)
#if HEAVY_DUTY_TESTING
#define LWSXParams ::testing::Values(1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 128, 256)
#define LWSYParams ::testing::Values(1, 2, 3, 4, 5, 6, 7, 8)
#define LWSZParams ::testing::Values(1, 2, 3, 4)
#else
#define LWSXParams ::testing::Values(1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 128, 256)
#define LWSYParams ::testing::Values(1, 2, 4, 8)
#define LWSZParams ::testing::Values(1)
#endif

INSTANTIATE_TEST_CASE_P(AllCombinations, LocalIDFixture, ::testing::Combine(SIMDParams, LWSXParams, LWSYParams, LWSZParams));
INSTANTIATE_TEST_CASE_P(LayoutTests, LocalIdsLayoutTest, SIMDParams);
INSTANTIATE_TEST_CASE_P(LayoutForImagesTests, LocalIdsLayoutForImagesTest, ::testing::Combine(SIMDParams, ::testing::Values(4, 8, 12, 20), ::testing::Values(4, 8, 12, 20)));

// To debug a specific configuration replace the list of Values with specific values.
// NOTE: You'll need a unique test prefix
INSTANTIATE_TEST_CASE_P(SingleTest, LocalIDFixture,
                        ::testing::Combine(
                            ::testing::Values(32),  //SIMD
                            ::testing::Values(5),   //LWSX
                            ::testing::Values(6),   //LWSY
                            ::testing::Values(7))); //LWSZ
