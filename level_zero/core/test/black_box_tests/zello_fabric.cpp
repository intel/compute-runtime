/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <iomanip>

bool showFabricConnectivityMatrix(ze_driver_handle_t &hDriver, bool isIncludeSubDevicesEnabled) {

    std::cout << " \n -- Displaying Fabric Connectivity matrix  -- \n";

    bool isIncludeDevices = true;

    uint32_t vertexCount = 0;
    std::vector<std::pair<ze_fabric_vertex_handle_t, char>> allVertices;
    SUCCESS_OR_TERMINATE(zeFabricVertexGetExp(hDriver, &vertexCount, nullptr));
    std::vector<ze_fabric_vertex_handle_t> vertices(vertexCount);
    SUCCESS_OR_TERMINATE(zeFabricVertexGetExp(hDriver, &vertexCount, vertices.data()));

    for (auto &vertex : vertices) {
        if (isIncludeDevices) {
            allVertices.push_back(std::make_pair(vertex, 'R'));
        }
        if (isIncludeSubDevicesEnabled) {
            uint32_t count = 0;
            SUCCESS_OR_TERMINATE(zeFabricVertexGetSubVerticesExp(vertex, &count, nullptr));
            std::vector<ze_fabric_vertex_handle_t> subVertices(count);
            SUCCESS_OR_TERMINATE(zeFabricVertexGetSubVerticesExp(vertex, &count, subVertices.data()));
            for (auto &subVertex : subVertices) {
                allVertices.push_back(std::make_pair(subVertex, 'S'));
            }
        }
    }

    if (allVertices.size() == 0) {
        std::cout << " -- No Fabric Vertices !!  -- \n";
        return false;
    }

    const uint32_t elementWidth = 15;
    // Print Header
    for (uint32_t i = 0; i < allVertices.size(); i++) {
        std::cout << std::setw(elementWidth) << "[" << allVertices[i].second << "]" << allVertices[i].first;
    }
    std::cout << "\n";

    for (uint32_t vertexAIndex = 0; vertexAIndex < allVertices.size(); vertexAIndex++) {
        std::cout << "[" << allVertices[vertexAIndex].second << "]" << allVertices[vertexAIndex].first;
        for (uint32_t vertexBIndex = 0; vertexBIndex < allVertices.size(); vertexBIndex++) {

            if (vertexAIndex == vertexBIndex) {
                std::cout << std::setw(elementWidth) << "X";
                continue;
            }
            uint32_t edgeCount = 0;
            SUCCESS_OR_TERMINATE(zeFabricEdgeGetExp(allVertices[vertexAIndex].first, allVertices[vertexBIndex].first, &edgeCount, nullptr));
            std::cout << std::setw(elementWidth) << edgeCount;
        }
        std::cout << "\n";
    }

    return true;
}

bool showFabricConnectivityProperties(ze_driver_handle_t &hDriver) {

    std::cout << " \n -- Displaying Fabric Connectivity Properties  -- \n";

    uint32_t vertexCount = 0;
    std::vector<std::pair<ze_fabric_vertex_handle_t, char>> allVertices;
    SUCCESS_OR_TERMINATE(zeFabricVertexGetExp(hDriver, &vertexCount, nullptr));
    std::vector<ze_fabric_vertex_handle_t> vertices(vertexCount);
    SUCCESS_OR_TERMINATE(zeFabricVertexGetExp(hDriver, &vertexCount, vertices.data()));

    for (auto &vertex : vertices) {
        allVertices.push_back(std::make_pair(vertex, 'R'));
        uint32_t count = 0;
        SUCCESS_OR_TERMINATE(zeFabricVertexGetSubVerticesExp(vertex, &count, nullptr));
        std::vector<ze_fabric_vertex_handle_t> subVertices(count);
        SUCCESS_OR_TERMINATE(zeFabricVertexGetSubVerticesExp(vertex, &count, subVertices.data()));
        for (auto &subVertex : subVertices) {
            allVertices.push_back(std::make_pair(subVertex, 'S'));
        }
    }

    if (allVertices.size() == 0) {
        std::cout << " -- No Fabric Vertices !!  -- \n";
        return false;
    }

    // Print the Sub Vertices and Vertices
    for (uint32_t i = 0; i < allVertices.size(); i++) {
        if (allVertices[i].second == 'S') {
            std::cout << "\t" << allVertices[i].first << "\n";
        } else {
            std::cout << allVertices[i].first << "\n";
        }
    }

    // Show properties of all edges
    for (uint32_t vertexAIndex = 0; vertexAIndex < allVertices.size(); vertexAIndex++) {
        for (uint32_t vertexBIndex = vertexAIndex + 1; vertexBIndex < allVertices.size(); vertexBIndex++) {
            uint32_t edgeCount = 0;
            SUCCESS_OR_TERMINATE(zeFabricEdgeGetExp(allVertices[vertexAIndex].first, allVertices[vertexBIndex].first, &edgeCount, nullptr));
            std::vector<ze_fabric_edge_handle_t> edges(edgeCount);
            SUCCESS_OR_TERMINATE(zeFabricEdgeGetExp(allVertices[vertexAIndex].first, allVertices[vertexBIndex].first, &edgeCount, edges.data()));

            if (edgeCount == 0) {
                continue;
            }

            std::cout << "Edge A: " << allVertices[vertexAIndex].first << " -- B: " << allVertices[vertexBIndex].first << "\n";
            for (auto &edge : edges) {
                ze_fabric_edge_exp_properties_t edgeProperties = {};
                SUCCESS_OR_TERMINATE(zeFabricEdgeGetPropertiesExp(edge, &edgeProperties));
                std::cout << "Uuid: ";
                for (uint32_t i = 0; i < ZE_MAX_UUID_SIZE; i++) {
                    std::cout << static_cast<uint32_t>(edgeProperties.uuid.id[i]) << " ";
                }
                std::cout << "\n";
                std::cout << "Model : " << edgeProperties.model << "\n";
                std::cout << "Bandwidth : " << edgeProperties.bandwidth << " | units: " << static_cast<uint32_t>(edgeProperties.bandwidthUnit) << "\n";
                std::cout << "Latency : " << edgeProperties.latency << " | units: " << static_cast<uint32_t>(edgeProperties.latencyUnit) << "\n";
                std::cout << "Duplexity : " << static_cast<uint32_t>(edgeProperties.duplexity) << "\n";
                std::cout << "\n";
            }
        }
        std::cout << " --- \n";
    }

    return true;
}

int main(int argc, char *argv[]) {

    const std::string blackBoxName = "Zello Fabric";
    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);

    const bool isSubDeviceDisplayEnabled = isParamEnabled(argc, argv, "-s", "--subDeviceEnable");
    bool status = true;

    status &= showFabricConnectivityMatrix(driverHandle, isSubDeviceDisplayEnabled);
    status &= showFabricConnectivityProperties(driverHandle);

    printResult(false, status, blackBoxName);
    return (status ? 0 : 1);
}
