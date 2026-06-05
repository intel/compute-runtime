/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/fabric/linux/fabric_create_edges_from_device_interfaces.h"

#include <vector>

namespace L0 {

void FabricEdge::createEdgesFromVertices(const std::vector<FabricVertex *> &vertices, std::vector<FabricEdge *> &edges, std::vector<FabricEdge *> &indirectEdges) {
    createEdgesFromFabricDeviceInterfaces(vertices, edges, indirectEdges);
}

} // namespace L0
