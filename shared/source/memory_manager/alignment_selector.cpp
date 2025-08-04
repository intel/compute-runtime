/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "alignment_selector.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/gfx_partition.h"

#include <algorithm>

namespace NEO {

bool operator==(const AlignmentSelector::CandidateAlignment &lhs, const AlignmentSelector::CandidateAlignment &rhs) {
    return std::tie(lhs.alignment, lhs.applyForSmallerSize, lhs.heap, lhs.maxMemoryWastage) ==
           std::tie(rhs.alignment, rhs.applyForSmallerSize, rhs.heap, rhs.maxMemoryWastage);
}

void AlignmentSelector::addCandidateAlignment(size_t alignment, bool applyForSmallerSize, float maxMemoryWastage) {
    this->addCandidateAlignment(alignment, applyForSmallerSize, maxMemoryWastage, HeapIndex::totalHeaps);
}

void AlignmentSelector::addCandidateAlignment(size_t alignment, bool applyForSmallerSize, float maxMemoryWastage, HeapIndex heap) {
    UNRECOVERABLE_IF(alignment == 0);
    UNRECOVERABLE_IF((alignment & (alignment - 1)) != 0); // must be power of 2

    CandidateAlignment candidateAlignment{};
    candidateAlignment.alignment = alignment;
    candidateAlignment.applyForSmallerSize = applyForSmallerSize;
    candidateAlignment.maxMemoryWastage = maxMemoryWastage;
    candidateAlignment.heap = heap;
    this->candidateAlignments.push_back(std::move(candidateAlignment));

    const auto comparator = [](const CandidateAlignment &left, const CandidateAlignment &right) {
        return left.alignment > right.alignment;
    };
    std::sort(this->candidateAlignments.begin(), this->candidateAlignments.end(), comparator);
}

AlignmentSelector::CandidateAlignment AlignmentSelector::selectAlignment(size_t size) const {
    constexpr size_t noAlignmentLimit = std::numeric_limits<size_t>::max();
    return this->selectAlignment(size, noAlignmentLimit);
}

AlignmentSelector::CandidateAlignment AlignmentSelector::selectAlignment(size_t size, size_t maximumPossibleAlignment) const {
    for (const CandidateAlignment &candidateAlignment : this->candidateAlignments) {
        if (!candidateAlignment.applyForSmallerSize && size < candidateAlignment.alignment) {
            continue;
        }

        if (candidateAlignment.alignment > maximumPossibleAlignment) {
            continue;
        }

        const size_t alignedSize = alignUp(size, candidateAlignment.alignment);
        const size_t wastedMemory = alignedSize - size;
        const size_t maxWastedMemory = static_cast<size_t>(alignedSize * candidateAlignment.maxMemoryWastage);
        if (wastedMemory > maxWastedMemory) {
            continue;
        }

        return candidateAlignment;
    }
    UNRECOVERABLE_IF(true);
}
} // namespace NEO
