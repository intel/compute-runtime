/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "gtest/gtest.h"

using namespace Math;

TEST(NextPowerOfTwo, WhenGettingNextPowerOfTwoThenCorrectValueIsReturned) {
    EXPECT_EQ(1u, nextPowerOfTwo(1U));
    EXPECT_EQ(2u, nextPowerOfTwo(2U));
    EXPECT_EQ(4u, nextPowerOfTwo(3U));
    EXPECT_EQ(32u, nextPowerOfTwo(31U));
    EXPECT_EQ(32u, nextPowerOfTwo(32U));
    EXPECT_EQ(64u, nextPowerOfTwo(33U));
    EXPECT_EQ(1u << 31, nextPowerOfTwo((1u << 30U) + 1));
    EXPECT_EQ(1u << 31, nextPowerOfTwo(1u << 31U));

    EXPECT_EQ(1ULL << 32, nextPowerOfTwo(static_cast<uint64_t>((1ULL << 31ULL) + 1)));
    EXPECT_EQ(1ULL << 32, nextPowerOfTwo(static_cast<uint64_t>(1ULL << 32ULL)));
}

TEST(PrevPowerOfTwo, WhenGettingPreviousPowerOfTwoThenCorrectValueIsReturned) {
    EXPECT_EQ(0u, prevPowerOfTwo(0U));
    EXPECT_EQ(1u, prevPowerOfTwo(1U));
    for (uint32_t i = 1; i < 32; i++) {
        uint32_t b = 1 << i;

        EXPECT_EQ(1u << (i - 1), prevPowerOfTwo(b - 1));
        EXPECT_EQ(b, prevPowerOfTwo(b));
        EXPECT_EQ(b, prevPowerOfTwo(b + 1));
    }

    EXPECT_EQ(1ULL << 32, prevPowerOfTwo(static_cast<uint64_t>(1ULL << 32ULL)));
    EXPECT_EQ(1ULL << 32, prevPowerOfTwo(static_cast<uint64_t>((1ULL << 32ULL) + 7)));
}

TEST(getMinLsbSet, WhenGettingMinLsbSetThenCorrectValueIsReturned) {
    // clang-format off
    EXPECT_EQ(0u,  getMinLsbSet(0x00000001u));
    EXPECT_EQ(1u,  getMinLsbSet(0x00000002u));
    EXPECT_EQ(16u, getMinLsbSet(0x40010000u));
    EXPECT_EQ(30u, getMinLsbSet(0x40000000u));
    EXPECT_EQ(31u, getMinLsbSet(0x80000000u));
    // clang-format on
}

TEST(getExponentWithLog2, GivenZeroWhenGettingLog2ThenSizeInBitsIsReturned) {
    // clang-format off
    EXPECT_EQ(32u,  log2((uint32_t)0u));
    EXPECT_EQ(64u,  log2((uint64_t)0u));
    // clang-format on
}

TEST(getExponentWithLog2, Given32BitIntegersWhenGettingLog2ThenLog2IsReturned) {
    // clang-format off
    EXPECT_EQ(0u,  log2((uint32_t)1u));
    EXPECT_EQ(1u,  log2((uint32_t)2u));
    EXPECT_EQ(2u,  log2((uint32_t)4u));
    EXPECT_EQ(3u,  log2((uint32_t)8u));
    EXPECT_EQ(4u,  log2((uint32_t)16u));
    EXPECT_EQ(10u, log2((uint32_t)1024u));
    EXPECT_EQ(31u, log2((uint32_t)2147483648u));
    // clang-format on
}

TEST(getExponentWithLog2, Given64BitIntegersWhenGettingLog2ThenLog2IsReturned) {
    // clang-format off
    EXPECT_EQ(0u,  log2((uint64_t)1u));
    EXPECT_EQ(1u,  log2((uint64_t)2u));
    EXPECT_EQ(2u,  log2((uint64_t)4u));
    EXPECT_EQ(3u,  log2((uint64_t)8u));
    EXPECT_EQ(4u,  log2((uint64_t)16u));
    EXPECT_EQ(10u, log2((uint64_t)1024u));
    EXPECT_EQ(31u, log2((uint64_t)2147483648u));
    EXPECT_EQ(41u, log2((uint64_t)2199023255552u));
    EXPECT_EQ(63u, log2((uint64_t)0x8000000000000000u));
    // clang-format on
}

TEST(getExponentWithLog2, GivenNonZeroWhenGettingLog2ThenLog2IsReturned) {
    // clang-format off
    EXPECT_EQ(1u,  log2(3u));
    EXPECT_EQ(2u,  log2(5u));
    EXPECT_EQ(2u,  log2(7u));
    EXPECT_EQ(3u,  log2(9u));
    EXPECT_EQ(3u,  log2(10u));
    EXPECT_EQ(10u, log2(1025u));
    // clang-format on
}

struct Float2HalfParams {
    float floatInput;
    uint16_t uintOutput;
};

// clang-format off
Float2HalfParams float2HalfParams[] = {
    {0.0f,                  0x0000},
    {1.0f,                  0x3c00},
    {PosInfinity.f,         0x7c00},
    {Nan.f,                 0x7e00},
    {std::ldexp(1.0f, 16),  0x7bff},
    {std::ldexp(1.0f, -25), 0x0000},
    {std::ldexp(1.0f, -15), 0x0200}};
// clang-format on

typedef ::testing::TestWithParam<Float2HalfParams> Float2HalfTest;

TEST_P(Float2HalfTest, WhenConvertingFloatToHalfThenValueIsPreserved) {
    float floatValue = GetParam().floatInput;
    uint16_t uint16ofHalf = float2Half(floatValue);
    uint16_t uintOutput = GetParam().uintOutput;
    EXPECT_EQ(uintOutput, uint16ofHalf);
}

INSTANTIATE_TEST_CASE_P(Float2Half,
                        Float2HalfTest,
                        ::testing::ValuesIn(float2HalfParams));

struct l3Config {
    union {
        unsigned int RawValue;
        struct {
            unsigned int SlmModeEnable : 1;
            unsigned int UrbAllocation : 7;
            unsigned int GpGpuCreditEnable : 1;
            unsigned int ErrorDetectionBehaviour : 1;
            unsigned int Reserved : 1;
            unsigned int ReadOnlyClientPool : 7;
            unsigned int DCWayAssignement : 7;
            unsigned int AllL3WayAssignement : 7;
        } bits;
    };
};

TEST(l3configsGenerator, givenInputValuesWhenPassedToL3ConfigThenRawValueIsProduced) {
    l3Config config;
    config.bits = {
        0,    // SLM Enabled
        0x30, // URB Allocation
        1,    // GPGPU L3 Credit Mode Enable
        0,    // Error Detection Behavior Control
        0,    // Reserved - MBZ
        0,    // Read-Only Client Pool
        0,    // DC-Way Assignment
        0x30};

    EXPECT_EQ(config.RawValue, 0x60000160u);

    l3Config config2;
    config2.RawValue = 0x80000140u;
    EXPECT_EQ(0x40u, config2.bits.AllL3WayAssignement);
    EXPECT_EQ(0x20u, config2.bits.UrbAllocation);
}

struct ElementCountsTestData {
    size_t x, y, z;
    size_t result;
};

ElementCountsTestData elementCountInputData[] = {
    {1, 2, 3, 6},
    {0, 0, 1, 1},
    {0, 1, 0, 1},
    {1, 0, 0, 1},
    {5, 0, 10, 50},
    {0, 0, 30, 30},
};

typedef ::testing::TestWithParam<ElementCountsTestData> ComputeTotalElementsCount;

TEST_P(ComputeTotalElementsCount, givenVariousInputVectorsWhenComputeTotalElementsCountIsUsedThenProperProductIsComputed) {
    Vec3<size_t> inputData(GetParam().x, GetParam().y, GetParam().z);
    EXPECT_EQ(GetParam().result, computeTotalElementsCount(inputData));
}

TEST(isPow2Test, WhenArgZeroThenReturnFalse) {
    EXPECT_FALSE(isPow2(0u));
}

TEST(isPow2Test, WhenArgNonPow2ThenReturnFalse) {
    EXPECT_FALSE(isPow2(3u));
    EXPECT_FALSE(isPow2(5u));
    EXPECT_FALSE(isPow2(6u));
    EXPECT_FALSE(isPow2(7u));
    EXPECT_FALSE(isPow2(10u));
}

TEST(isPow2Test, WhenArgPow2ThenReturnTrue) {
    EXPECT_TRUE(isPow2(1u));
    EXPECT_TRUE(isPow2(4u));
    EXPECT_TRUE(isPow2(8u));
    EXPECT_TRUE(isPow2(128u));
    EXPECT_TRUE(isPow2(4096u));
}

TEST(ffs, givenZeroThenReturnMaxRange) {
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), ffs(0U));
}

TEST(ffs, givenNonZeroThenReturnFirstSetBitIndex) {
    EXPECT_EQ(0U, ffs(0b1U));
    EXPECT_EQ(0U, ffs(0b11U));
    EXPECT_EQ(1U, ffs(0b10U));
    EXPECT_EQ(3U, ffs(0b1001000U));
    EXPECT_EQ(31U, ffs(1U << 31U));
    EXPECT_EQ(16U, ffs((1U << 31U) | (1U << 31U) | (1U << 16U)));
    EXPECT_EQ(16ULL, ffs((1ULL << 63ULL) | (1ULL << 32ULL) | (1ULL << 16ULL)));
    EXPECT_EQ(63ULL, ffs(1ULL << 63ULL));
}
