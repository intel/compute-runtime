/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/experimental/source/graph/graph.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

struct MockGraph : Graph {
    using Graph::captureTargetDesc;
    using Graph::Graph;
    using Graph::recordedSignals;
};

struct MockExecutableGraph : ExecutableGraph {
    using ExecutableGraph::ExecutableGraph;
    using ExecutableGraph::myCommandLists;
    using ExecutableGraph::myOrderedSegments;
    using ExecutableGraph::usePatchingPreamble;
};

} // namespace ult
} // namespace L0
