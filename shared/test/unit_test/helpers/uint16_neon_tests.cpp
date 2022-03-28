/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aarch64/uint16_neon.h"
#include "shared/source/helpers/aligned_memory.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(Uint16Neon, GivenNeonAndMaskWhenCastingToBoolThenTrueIsReturned) {
    EXPECT_TRUE(static_cast<bool>(NEO::uint16x16_t::mask()));
}

TEST(Uint16Neon, GivenNeonAndZeroWhenCastingToBoolThenFalseIsReturned) {
    EXPECT_FALSE(static_cast<bool>(NEO::uint16x16_t::zero()));
}

TEST(Uint16Neon, GivenNeonWhenConjoiningMaskAndZeroThenBooleanResultIsCorrect) {
    EXPECT_TRUE(NEO::uint16x16_t::mask() && NEO::uint16x16_t::mask());
    EXPECT_FALSE(NEO::uint16x16_t::mask() && NEO::uint16x16_t::zero());
    EXPECT_FALSE(NEO::uint16x16_t::zero() && NEO::uint16x16_t::mask());
    EXPECT_FALSE(NEO::uint16x16_t::zero() && NEO::uint16x16_t::zero());
}

TEST(Uint16Neon, GivenNeonAndOneWhenCreatingThenInstancesAreSame) {
    auto one = NEO::uint16x16_t::one();
    NEO::uint16x16_t alsoOne(one);
    EXPECT_EQ(0, memcmp(&alsoOne, &one, sizeof(NEO::uint16x16_t)));
}

TEST(Uint16Neon, GivenNeonAndValueWhenCreatingThenConstructorIsReplicated) {
    NEO::uint16x16_t allSevens(7u);
    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(7u, allSevens.get(i));
    }
}

ALIGNAS(32)
static const uint16_t laneValues[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

TEST(Uint16Neon, GivenNeonAndArrayWhenCreatingThenConstructorIsReplicated) {
    NEO::uint16x16_t lanes(laneValues);
    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i), lanes.get(i));
    }
}

TEST(Uint16Neon, GivenNeonWhenLoadingThenValuesAreSetCorrectly) {
    NEO::uint16x16_t lanes;
    lanes.load(laneValues);
    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i), lanes.get(i));
    }
}

TEST(Uint16Neon, GivenNeonWhenStoringThenValuesAreSetCorrectly) {
    uint16_t *alignedMemory = reinterpret_cast<uint16_t *>(alignedMalloc(1024, 32));

    NEO::uint16x16_t lanes(laneValues);
    lanes.store(alignedMemory);
    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i), alignedMemory[i]);
    }

    alignedFree(alignedMemory);
}

TEST(Uint16Neon, GivenNeonWhenDecrementingThenValuesAreSetCorrectly) {
    NEO::uint16x16_t result(laneValues);
    result -= NEO::uint16x16_t::one();

    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i - 1), result.get(i));
    }
}

TEST(Uint16Neon, GivenNeonWhenIncrementingThenValuesAreSetCorrectly) {
    NEO::uint16x16_t result(laneValues);
    result += NEO::uint16x16_t::one();

    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i + 1), result.get(i));
    }
}

TEST(Uint16Sse4, GivenNeonWhenBlendingThenValuesAreSetCorrectly) {
    NEO::uint16x16_t a(NEO::uint16x16_t::one());
    NEO::uint16x16_t b(NEO::uint16x16_t::zero());
    NEO::uint16x16_t c;

    // c = mask ? a : b
    c = blend(a, b, NEO::uint16x16_t::mask());

    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(a.get(i), c.get(i));
    }

    // c = mask ? a : b
    c = blend(a, b, NEO::uint16x16_t::zero());

    for (int i = 0; i < NEO::uint16x16_t::numChannels; ++i) {
        EXPECT_EQ(b.get(i), c.get(i));
    }
}
