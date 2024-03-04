/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fabric/fabric.h"

#include <vector>

namespace L0 {

void FabricEdge::createEdgesFromVertices(const std::vector<FabricVertex *> &vertices, std::vector<FabricEdge *> &edges, std::vector<FabricEdge *> &) {

    // Get all vertices and sub-vertices
    std::vector<FabricVertex *> allVertices = {};
    for (auto &fabricVertex : vertices) {
        allVertices.push_back(fabricVertex);
        for (auto &fabricSubVertex : fabricVertex->subVertices) {
            allVertices.push_back(fabricSubVertex);
        }
    }

    // Get direct physical edges between all vertices
    for (uint32_t vertexAIndex = 0; vertexAIndex < allVertices.size(); vertexAIndex++) {
        for (uint32_t vertexBIndex = vertexAIndex + 1; vertexBIndex < allVertices.size(); vertexBIndex++) {
            auto vertexA = allVertices[vertexAIndex];
            auto vertexB = allVertices[vertexBIndex];
            ze_fabric_edge_exp_properties_t edgeProperty = {};

            for (auto const &fabricDeviceInterface : vertexA->pFabricDeviceInterfaces) {
                bool isConnected =
                    fabricDeviceInterface.second->getEdgeProperty(vertexB, edgeProperty);
                if (isConnected) {
                    edges.push_back(create(vertexA, vertexB, edgeProperty));
                }
            }
        }
    }
}

} // namespace L0
