/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fabric/fabric.h"

#include "shared/source/helpers/debug_helpers.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace L0 {

void FabricEdge::createEdgesFromVertices(const std::vector<FabricVertex *> &vertices, std::vector<FabricEdge *> &edges, std::vector<FabricEdge *> &indirectEdges) {

    // Get all vertices and sub-vertices
    std::vector<FabricVertex *> allVertices = {};
    for (auto &fabricVertex : vertices) {
        allVertices.push_back(fabricVertex);
        for (auto &fabricSubVertex : fabricVertex->subVertices) {
            allVertices.push_back(fabricSubVertex);
        }
    }

    // Get direct physical edges between all vertices
    std::map<uint32_t, std::vector<std::pair<uint32_t, ze_fabric_edge_exp_properties_t *>>> adjacentVerticesMap;
    std::map<uint32_t, std::vector<uint32_t>> nonAdjacentVerticesMap;
    for (uint32_t vertexAIndex = 0; vertexAIndex < allVertices.size(); vertexAIndex++) {
        for (uint32_t vertexBIndex = vertexAIndex + 1; vertexBIndex < allVertices.size(); vertexBIndex++) {
            bool isAdjacent = false;
            auto vertexA = allVertices[vertexAIndex];
            auto vertexB = allVertices[vertexBIndex];
            ze_fabric_edge_exp_properties_t edgeProperty = {};

            for (auto const &fabricDeviceInterface : vertexA->pFabricDeviceInterfaces) {
                bool isConnected =
                    fabricDeviceInterface.second->getEdgeProperty(vertexB, edgeProperty);
                if (isConnected) {
                    edges.push_back(create(vertexA, vertexB, edgeProperty));
                    adjacentVerticesMap[vertexAIndex].emplace_back(vertexBIndex, &edges.back()->properties);
                    adjacentVerticesMap[vertexBIndex].emplace_back(vertexAIndex, &edges.back()->properties);
                    isAdjacent = true;
                }
            }
            if (!isAdjacent) {
                auto &subVerticesOfA = vertexA->subVertices;
                if (std::find(subVerticesOfA.begin(), subVerticesOfA.end(), vertexB) == subVerticesOfA.end()) {
                    nonAdjacentVerticesMap[vertexAIndex].push_back(vertexBIndex);
                    nonAdjacentVerticesMap[vertexBIndex].push_back(vertexAIndex);
                }
            }
        }
    }

    // Find logical multi-hop edges between vertices not directly connected
    for (const auto &[vertexAIndex, nonAdjacentVertices] : nonAdjacentVerticesMap) {
        for (auto vertexBIndex : nonAdjacentVertices) {
            std::map<uint32_t, uint32_t> visited;
            visited[vertexAIndex] = vertexAIndex;

            std::deque<uint32_t> toVisit;
            toVisit.push_back(vertexAIndex);

            uint32_t currVertexIndex = vertexAIndex;

            while (true) {
                std::deque<uint32_t> toVisitIaf, toVisitMdfi;
                while (!toVisit.empty()) {
                    currVertexIndex = toVisit.front();
                    toVisit.pop_front();
                    if (currVertexIndex == vertexBIndex) {
                        break;
                    }

                    for (auto [vertexIndex, edgeProperty] : adjacentVerticesMap[currVertexIndex]) {
                        if (visited.find(vertexIndex) == visited.end()) {
                            if (strncmp(edgeProperty->model, "XeLink", 7) == 0) {
                                toVisitIaf.push_back(vertexIndex);
                            } else {
                                DEBUG_BREAK_IF(strncmp(edgeProperty->model, "MDFI", 5) != 0);
                                toVisitMdfi.push_back(vertexIndex);
                            }
                            visited[vertexIndex] = currVertexIndex;
                        }
                    }
                }

                if (currVertexIndex != vertexBIndex) {
                    if (toVisitIaf.size() + toVisitMdfi.size() != 0) {
                        toVisit.insert(toVisit.end(), toVisitMdfi.begin(), toVisitMdfi.end());
                        toVisit.insert(toVisit.end(), toVisitIaf.begin(), toVisitIaf.end());
                    } else {
                        break;
                    }
                } else {
                    bool hasMdfi = false;
                    std::string path = "";
                    ze_fabric_edge_exp_properties_t properties = {};
                    properties.stype = ZE_STRUCTURE_TYPE_FABRIC_EDGE_EXP_PROPERTIES;
                    properties.pNext = nullptr;
                    memset(properties.uuid.id, 0, ZE_MAX_UUID_SIZE);
                    memset(properties.model, 0, ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE);
                    properties.bandwidth = std::numeric_limits<uint32_t>::max();
                    properties.bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
                    properties.latency = 0;
                    properties.latencyUnit = ZE_LATENCY_UNIT_HOP;
                    properties.duplexity = ZE_FABRIC_EDGE_EXP_DUPLEXITY_FULL_DUPLEX;

                    while (true) {
                        const auto parentIndex = visited[currVertexIndex];
                        ze_fabric_edge_exp_properties_t *currEdgeProperty = nullptr;
                        for (const auto &[vertexIndex, edgeProperty] : adjacentVerticesMap[parentIndex]) {
                            if (vertexIndex == currVertexIndex) {
                                currEdgeProperty = edgeProperty;
                                break;
                            }
                        }
                        UNRECOVERABLE_IF(currEdgeProperty == nullptr);
                        path = std::string(currEdgeProperty->model) + path;
                        if (strncmp(currEdgeProperty->model, "XeLink", 7) == 0) {
                            if (currEdgeProperty->bandwidth < properties.bandwidth) {
                                properties.bandwidth = currEdgeProperty->bandwidth;
                            }
                            properties.latency += currEdgeProperty->latency;
                        }

                        if (strncmp(currEdgeProperty->model, "MDFI", 5) == 0) {
                            hasMdfi = true;
                        }

                        currVertexIndex = parentIndex;
                        if (currVertexIndex == vertexAIndex) {
                            path.resize(ZE_MAX_FABRIC_EDGE_MODEL_EXP_SIZE - 1, '\0');
                            path.copy(properties.model, path.size());
                            break;
                        } else {
                            path = '-' + path;
                        }
                    }
                    if (hasMdfi) {
                        properties.latency = 0;
                        properties.latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;
                    }
                    indirectEdges.push_back(create(allVertices[vertexAIndex], allVertices[vertexBIndex], properties));
                    break;
                }
            }
        }
    }
}

} // namespace L0
