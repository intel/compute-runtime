/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/memory_manager/residency_container.h"

#include <atomic>

namespace OCLRT {

class GraphicsAllocation;
class WddmAllocation;

class WddmResidencyController {
  public:
    WddmResidencyController();

    void acquireLock();
    void releaseLock();

    WddmAllocation *getTrimCandidateHead();
    void addToTrimCandidateList(GraphicsAllocation *allocation);
    void removeFromTrimCandidateList(GraphicsAllocation *allocation, bool compactList);
    void checkTrimCandidateCount();

    bool checkTrimCandidateListCompaction();
    void compactTrimCandidateList();

    uint64_t getLastTrimFenceValue() { return lastTrimFenceValue; }
    void setLastTrimFenceValue(uint64_t value) { lastTrimFenceValue = value; }
    const ResidencyContainer &peekTrimCandidateList() const { return trimCandidateList; }
    uint32_t peekTrimCandidatesCount() const { return trimCandidatesCount; }

  protected:
    std::atomic<bool> lock;
    uint64_t lastTrimFenceValue;
    ResidencyContainer trimCandidateList;
    uint32_t trimCandidatesCount = 0;
};
} // namespace OCLRT
