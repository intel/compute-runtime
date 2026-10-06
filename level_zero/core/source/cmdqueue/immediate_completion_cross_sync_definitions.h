/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <vector>

namespace NEO {
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct ExecutableGraph;

struct ImmediateCountersCrossSync {
    uint64_t counter = 0;
    uint64_t deviceGpuAddress = 0;
    NEO::GraphicsAllocation *deviceGraphicsAllocation = nullptr;
    uint32_t devicePartitionCount = 1;
};

struct ExecutionSegmentImmediateCountersCrossSync : ImmediateCountersCrossSync {
    ExecutableGraph *segmentIdentifier = nullptr;
};

using ImmediateCountersCrossSyncList = std::vector<ImmediateCountersCrossSync>;
using ExecutionSegmentImmediateCountersCrossSyncList = std::vector<ExecutionSegmentImmediateCountersCrossSync>;

} // namespace L0
