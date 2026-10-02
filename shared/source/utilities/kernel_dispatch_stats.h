/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace NEO {

struct KernelDispatchStats {
    std::string kernelName;
    uint64_t globalWorkSize[3] = {0U, 0U, 0U};
    uint32_t localWorkSize[3] = {0U, 0U, 0U};
    uint32_t simdSize = 0U;
    uint32_t numGrfRequired = 0U;
    uint32_t slmInlineSize = 0U;
    uint32_t slmTotalSizePerThreadGroup = 0U;
    uint32_t barrierCount = 0U;
    uint32_t perThreadScratchSize[2] = {0U, 0U};
    uint32_t threadsPerThreadGroup = 0U;
    uint32_t threadGroupCount = 0U;
    bool usesSystolicMode = false;
    bool isIndirect = false;
};

class KernelDispatchStatsTracker : NEO::NonCopyableAndNonMovableClass {
  public:
    void trackDispatch(const KernelDispatchStats &stats);
    void merge(KernelDispatchStatsTracker &source);
    bool isEmpty();
    void clear();
    std::string createReport();

  private:
    using DispatchCounts = std::unordered_map<std::string, uint64_t>;

    DispatchCounts takeSnapshot();

    std::mutex mutex;
    DispatchCounts dispatchCounts;
};
static_assert(NEO::NonCopyableAndNonMovable<KernelDispatchStatsTracker>);

} // namespace NEO
