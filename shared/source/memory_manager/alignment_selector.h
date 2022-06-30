/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/gfx_partition.h"

#include <vector>

namespace NEO {

class AlignmentSelector {
  public:
    constexpr static inline float anyWastage = 1.f;
    constexpr static inline float noWastage = 0.f;

    struct CandidateAlignment {
        size_t alignment;         // selected alignment for the allocation
        bool applyForSmallerSize; // if enabled, this alignment can be used even when size < alignment
        float maxMemoryWastage;   // maximum percent of wasted memory after alignUp(size, alignment)
        HeapIndex heap;           // GPUVA heap, which allocation should go to
    };

    AlignmentSelector() = default;

    void addCandidateAlignment(size_t alignment, bool applyForSmallerSize, float maxMemoryWastage);
    void addCandidateAlignment(size_t alignment, bool applyForSmallerSize, float maxMemoryWastage, HeapIndex heap);

    CandidateAlignment selectAlignment(size_t size) const;
    auto &peekCandidateAlignments() const { return candidateAlignments; }

  private:
    std::vector<CandidateAlignment> candidateAlignments; // sorted by alignment size
};

bool operator==(const AlignmentSelector::CandidateAlignment &left, const AlignmentSelector::CandidateAlignment &right);

} // namespace NEO
