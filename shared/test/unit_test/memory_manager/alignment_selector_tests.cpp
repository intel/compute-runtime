/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/alignment_selector.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

TEST(AlignmentSelectorTests, givenNoCandidateAlignmentsWhenSelectingAlignmentThenFail) {
    AlignmentSelector selector{};
    EXPECT_ANY_THROW(selector.selectAlignment(1));
    EXPECT_ANY_THROW(selector.selectAlignment(1024));
}

TEST(AlignmentSelectorTests, givenNonPowerOfTwoAlignmentAddedThenFail) {
    AlignmentSelector selector{};
    EXPECT_ANY_THROW(selector.addCandidateAlignment(0, false, AlignmentSelector::anyWastage));
    EXPECT_ANY_THROW(selector.addCandidateAlignment(3, false, AlignmentSelector::anyWastage));
    EXPECT_ANY_THROW(selector.addCandidateAlignment(33, false, AlignmentSelector::anyWastage));
    EXPECT_ANY_THROW(selector.addCandidateAlignment(100, false, AlignmentSelector::anyWastage));
}

TEST(AlignmentSelectorTests, givenNoMatchingCandidateAlignmentsWhenSelectingAlignmentThenFail) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(128, false, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(512, false, AlignmentSelector::anyWastage);

    EXPECT_ANY_THROW(selector.selectAlignment(1));
    EXPECT_ANY_THROW(selector.selectAlignment(127));
}

TEST(AlignmentSelectorTests, givenApplyForSmallerSizeDisabledAndAllocationIsTooSmallWhenSelectingAlignmentThenDoNotUseTheAlignment) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(1, false, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(128, false, AlignmentSelector::anyWastage);

    EXPECT_EQ(1u, selector.selectAlignment(1).alignment);
    EXPECT_EQ(1u, selector.selectAlignment(127).alignment);
}

TEST(AlignmentSelectorTests, givenApplyForSmallerSizeEnabledAndAllocationIsTooSmallWhenSelectingAlignmentThenUseTheAlignment) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(1, true, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(128, true, AlignmentSelector::anyWastage);

    EXPECT_EQ(128u, selector.selectAlignment(1).alignment);
    EXPECT_EQ(128u, selector.selectAlignment(127).alignment);
}

TEST(AlignmentSelectorTests, givenMaximimumPossibleAlignmentWhenSelectAlignmentThenDoNotSelectGreaterThanLimit) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(1024, false, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(2048, false, AlignmentSelector::anyWastage);

    EXPECT_EQ(2048u, selector.selectAlignment(90000, std::numeric_limits<size_t>::max()).alignment);
    EXPECT_EQ(1024u, selector.selectAlignment(90000, 1024u).alignment);
}

TEST(AlignmentSelectorTests, givenMultipleMatchingCandidateAlignmentsWhenSelectingAlignmentThenSelectTheBiggest) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(1024, false, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(2048, false, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(128, false, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(512, false, AlignmentSelector::anyWastage);

    EXPECT_EQ(2048u, selector.selectAlignment(90000).alignment);
    EXPECT_EQ(2048u, selector.selectAlignment(2048).alignment);
    EXPECT_EQ(1024u, selector.selectAlignment(2047).alignment);
}

TEST(AlignmentSelectorTests, givenMaxWastageThresholdWhenSelectingAlignmentThenRespectTheRestriction) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(1, false, AlignmentSelector::anyWastage);
    selector.addCandidateAlignment(128, false, 0.25f);

    EXPECT_EQ(128u, selector.selectAlignment(256).alignment); // waste = 0%
    EXPECT_EQ(128u, selector.selectAlignment(193).alignment); // waste < 25%
    EXPECT_EQ(128u, selector.selectAlignment(192).alignment); // waste = 25%
    EXPECT_EQ(1u, selector.selectAlignment(191).alignment);   // waste > 25%, do not allow
    EXPECT_EQ(1u, selector.selectAlignment(129).alignment);   // waste > 25%, do not allow
    EXPECT_EQ(128u, selector.selectAlignment(128).alignment); // waste = 0%

    EXPECT_EQ(128u, selector.selectAlignment(289).alignment); // waste < 20%
    EXPECT_EQ(128u, selector.selectAlignment(288).alignment); // waste = 20%
    EXPECT_EQ(1u, selector.selectAlignment(287).alignment);   // waste > 20%, do not allow
}

TEST(AlignmentSelectorTests, givenCandidateArgumentHeapNotProvidedWhenSelectingAlignmentThenDefaultToUnknownHeap) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(1, false, AlignmentSelector::anyWastage);
    EXPECT_EQ(HeapIndex::totalHeaps, selector.selectAlignment(1).heap);
}

TEST(AlignmentSelectorTests, givenCandidateArgumentHeapProvidedWhenSelectingAlignmentThenReturnTheHeap) {
    AlignmentSelector selector{};
    selector.addCandidateAlignment(1, false, AlignmentSelector::anyWastage, HeapIndex::heapInternal);
    selector.addCandidateAlignment(16, false, AlignmentSelector::anyWastage, HeapIndex::heapExternalDeviceMemory);
    selector.addCandidateAlignment(32, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB);

    EXPECT_EQ(HeapIndex::heapInternal, selector.selectAlignment(1).heap);
    EXPECT_EQ(HeapIndex::heapExternalDeviceMemory, selector.selectAlignment(16).heap);
    EXPECT_EQ(HeapIndex::heapStandard2MB, selector.selectAlignment(32).heap);
}

} // namespace NEO
