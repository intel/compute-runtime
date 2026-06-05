/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <vector>

namespace L0 {
struct FabricVertex;
struct FabricEdge;

void createEdgesFromFabricDeviceInterfaces(const std::vector<FabricVertex *> &vertices,
                                           std::vector<FabricEdge *> &edges,
                                           std::vector<FabricEdge *> &indirectEdges);

} // namespace L0
