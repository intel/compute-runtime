/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/l3_range.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

constexpr uint64_t l3RangeMinimumAlignment = MemoryConstants::pageSize;
constexpr uint64_t l3RangeMax = 4 * MemoryConstants::gigaByte;
const uint64_t defaultPolicy = 0;

TEST(L3Range, whenMeetsMinimumAlignmentThenTrueIsReturned) {
    EXPECT_TRUE(L3Range::meetsMinimumAlignment(0));
    EXPECT_TRUE(L3Range::meetsMinimumAlignment(l3RangeMinimumAlignment));
    EXPECT_TRUE(L3Range::meetsMinimumAlignment(l3RangeMinimumAlignment * 2));
    EXPECT_TRUE(L3Range::meetsMinimumAlignment(l3RangeMinimumAlignment * 3));
    EXPECT_TRUE(L3Range::meetsMinimumAlignment(l3RangeMinimumAlignment * 4));
}

TEST(L3Range, whenDoesNotMeetMinimumAlignmentThenFalseIsReturned) {
    EXPECT_FALSE(L3Range::meetsMinimumAlignment(1));
    EXPECT_FALSE(L3Range::meetsMinimumAlignment(l3RangeMinimumAlignment - 1));
}

TEST(L3Range, whenValidSizeThenProperMaskFromSizeIsReturned) {
    EXPECT_EQ(0U, L3Range::getMaskFromSize(l3RangeMinimumAlignment));
    EXPECT_EQ(1U, L3Range::getMaskFromSize(l3RangeMinimumAlignment * 2));
    EXPECT_EQ(2U, L3Range::getMaskFromSize(l3RangeMinimumAlignment * 4));
    EXPECT_EQ(3U, L3Range::getMaskFromSize(l3RangeMinimumAlignment * 8));

    EXPECT_EQ(19U, L3Range::getMaskFromSize(l3RangeMax / 2));
    EXPECT_EQ(20U, L3Range::getMaskFromSize(l3RangeMax));
}

TEST(L3Range, whenNonPow2SizeThenMaskCalculationIsAborted) {
    EXPECT_THROW(L3Range::getMaskFromSize(l3RangeMinimumAlignment + 1), std::exception);
}

TEST(L3Range, whenTooSmallSizeThenMaskCalculationIsAborted) {
    EXPECT_THROW(L3Range::getMaskFromSize(0), std::exception);
}

TEST(L3Range, whenTooBigSizeThenMaskCalculationIsAborted) {
    EXPECT_THROW(L3Range::getMaskFromSize(l3RangeMax * 2), std::exception);
}

TEST(L3Range, WhenGettingMaskFromSizeThenCorrectSizeIsReturned) {
    L3Range range;

    range.setMask(L3Range::getMaskFromSize(l3RangeMinimumAlignment));
    EXPECT_EQ(l3RangeMinimumAlignment, range.getSizeInBytes());

    range.setMask(L3Range::getMaskFromSize(l3RangeMinimumAlignment * 4));
    EXPECT_EQ(l3RangeMinimumAlignment * 4, range.getSizeInBytes());

    range.setMask(L3Range::getMaskFromSize(l3RangeMax));
    EXPECT_EQ(l3RangeMax, range.getSizeInBytes());
}

TEST(L3Range, whenMaskGetsChangedThenReturnsProperlyMaskedAddress) {
    L3Range range;
    range.setAddress(l3RangeMinimumAlignment * 4 + l3RangeMinimumAlignment * 3 + 1);

    range.setMask(0);
    EXPECT_EQ(range.getAddress() & ~(l3RangeMinimumAlignment - 1), range.getMaskedAddress());

    range.setMask(1);
    EXPECT_EQ(range.getAddress() & ~((l3RangeMinimumAlignment << 1) - 1), range.getMaskedAddress());

    range.setMask(2);
    EXPECT_EQ(range.getAddress() & ~((l3RangeMinimumAlignment << 2) - 1), range.getMaskedAddress());

    range.setMask(3);
    EXPECT_EQ(0U, range.getMaskedAddress());
}

TEST(L3Range, whenCreatedFromAddressAndMaskThenAddressAndMaskAreProperlySet) {
    {
        L3Range range = L3Range::fromAddressMask(0U, 0U);
        EXPECT_EQ(0U, range.getAddress());
        EXPECT_EQ(0U, range.getMask());
    }

    {
        L3Range range = L3Range::fromAddressMask(l3RangeMinimumAlignment, 1U);
        EXPECT_EQ(l3RangeMinimumAlignment, range.getAddress());
        EXPECT_EQ(1U, range.getMask());
    }

    {
        L3Range range = L3Range::fromAddressMask(l3RangeMinimumAlignment * 2, 3U);
        EXPECT_EQ(l3RangeMinimumAlignment * 2, range.getAddress());
        EXPECT_EQ(3U, range.getMask());
    }
}

TEST(L3Range, whenCreatedFromAddressAndSizeThenMaskIsProperlySet) {
    {
        L3Range range = L3Range::fromAddressSize(0, l3RangeMinimumAlignment);
        EXPECT_EQ(0U, range.getAddress());
        EXPECT_EQ(L3Range::getMaskFromSize(l3RangeMinimumAlignment), range.getMask());
    }

    {
        L3Range range = L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment * 2);
        EXPECT_EQ(l3RangeMinimumAlignment, range.getAddress());
        EXPECT_EQ(L3Range::getMaskFromSize(l3RangeMinimumAlignment * 2), range.getMask());
    }

    {
        L3Range range = L3Range::fromAddressSize(l3RangeMinimumAlignment * 2, l3RangeMax);
        EXPECT_EQ(l3RangeMinimumAlignment * 2, range.getAddress());
        EXPECT_EQ(L3Range::getMaskFromSize(l3RangeMax), range.getMask());
    }
}

TEST(L3Range, whenComparOpIsEqualThenReturnsTrueOnlyIfSame) {
    L3Range a = L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment * 2);
    L3Range b = L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment * 2);
    L3Range c = L3Range::fromAddressSize(0, l3RangeMinimumAlignment * 2);
    L3Range d = L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(c == a);
    EXPECT_FALSE(a == d);
    EXPECT_FALSE(d == a);
}

TEST(L3Range, whenComparOpIsNotEqualThenReturnsTrueOnlyIfNotSame) {
    L3Range a = L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment * 2);
    L3Range b = L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment * 2);
    L3Range c = L3Range::fromAddressSize(0, l3RangeMinimumAlignment * 2);
    L3Range d = L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment);

    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(c != a);
    EXPECT_TRUE(a != d);
    EXPECT_TRUE(d != a);
}

TEST(CoverRange, whenNonAlignedThenAbort) {
    L3RangesVec ranges;
    EXPECT_THROW(coverRangeExact(1, l3RangeMinimumAlignment, ranges, defaultPolicy), std::exception);
    EXPECT_THROW(coverRangeExact(l3RangeMinimumAlignment, 1, ranges, defaultPolicy), std::exception);
    EXPECT_THROW(coverRangeExact(1, 1, ranges, defaultPolicy), std::exception);
}

L3Range fromAdjacentRange(const L3Range &lhs, uint64_t size) {
    L3Range ret;
    ret.setAddress(lhs.getMaskedAddress() + lhs.getSizeInBytes());
    ret.setMask(L3Range::getMaskFromSize(size));
    return ret;
}

TEST(CoverRange, whenAlignedThenCoverWithProperSubranges) {
    {
        L3RangesVec actualRanges;
        coverRangeExact(0, l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(0, l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "1 page, offset 0";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(l3RangeMinimumAlignment, l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "1 page, offset 1";
    }

    // 2 pages
    {
        L3RangesVec actualRanges;
        coverRangeExact(0, 2 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(0, 2 * l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "2 pages, offset 0";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(l3RangeMinimumAlignment, 2 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(2 * l3RangeMinimumAlignment, l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "2 pages, offset 1";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(0, 3 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(0, 2 * l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(2 * l3RangeMinimumAlignment, l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "3 pages, offset 0";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(l3RangeMinimumAlignment, 3 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(2 * l3RangeMinimumAlignment, 2 * l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "3 pages, offset 1";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(2 * l3RangeMinimumAlignment, 3 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(2 * l3RangeMinimumAlignment, 2 * l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(4 * l3RangeMinimumAlignment, l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "3 pages, offset 2";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(0, 4 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(0, 4 * l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "4 pages, offset 0";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(l3RangeMinimumAlignment, 4 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(l3RangeMinimumAlignment, l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(2 * l3RangeMinimumAlignment, 2 * l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(4 * l3RangeMinimumAlignment, l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "4 pages, offset 1";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(2 * l3RangeMinimumAlignment, 4 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(2 * l3RangeMinimumAlignment, 2 * l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(4 * l3RangeMinimumAlignment, 2 * l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "4 pages, offset 2";
    }

    {
        L3RangesVec actualRanges;
        coverRangeExact(3 * l3RangeMinimumAlignment, 4 * l3RangeMinimumAlignment, actualRanges, defaultPolicy);

        L3RangesVec expectedRanges{L3Range::fromAddressSize(3 * l3RangeMinimumAlignment, l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(4 * l3RangeMinimumAlignment, 2 * l3RangeMinimumAlignment),
                                   L3Range::fromAddressSize(6 * l3RangeMinimumAlignment, l3RangeMinimumAlignment)};
        EXPECT_EQ(expectedRanges, actualRanges) << "4 pages, offset 3";
    }

    {
        uint64_t address = 3 * 4096;
        uint64_t size = 1024 * 1024;

        L3RangesVec actualRanges;
        coverRangeExact(address, size, actualRanges, 0);

        L3RangesVec expectedRanges;
        expectedRanges.push_back(L3Range::fromAddressSize(address, 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 4 * 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 8 * 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 16 * 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 32 * 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 64 * 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 128 * 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 2 * 4096));
        expectedRanges.push_back(fromAdjacentRange(*expectedRanges.rbegin(), 1 * 4096));

        EXPECT_EQ(expectedRanges, actualRanges) << "1MB, offset 3 pages";
    }
}
TEST(CoverRange, whenRangeCreatedWithPolicyThenAllParamsSetCorrectly) {
    L3Range range = L3Range::fromAddressSizeWithPolicy(0, l3RangeMinimumAlignment, defaultPolicy);
    EXPECT_EQ(0U, range.getAddress());
    EXPECT_EQ(L3Range::getMaskFromSize(l3RangeMinimumAlignment), range.getMask());
    EXPECT_EQ(range.getPolicy(), defaultPolicy);

    auto policy = defaultPolicy + 1;
    L3Range range2 = L3Range::fromAddressSizeWithPolicy(0, l3RangeMinimumAlignment, policy);
    EXPECT_EQ(range2.getPolicy(), policy);
}
