/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/fabric/fabric_device_interface.h"
#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <map>
#include <memory>
#include <vector>

struct _ze_fabric_vertex_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_fabric_vertex_handle_t>);
struct _ze_fabric_edge_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_fabric_edge_handle_t>);

namespace L0 {
struct Device;
struct DriverHandle;

struct FabricVertex : _ze_fabric_vertex_handle_t {

  public:
    static FabricVertex *createFromDevice(Device *device);
    virtual ~FabricVertex();
    ze_result_t getSubVertices(uint32_t *pCount, ze_fabric_vertex_handle_t *phSubvertices);
    ze_result_t getProperties(ze_fabric_vertex_exp_properties_t *pVertexProperties) const;
    ze_result_t getDevice(ze_device_handle_t *phDevice) const;
    static FabricVertex *fromHandle(ze_fabric_vertex_handle_t handle) { return static_cast<FabricVertex *>(handle); }
    inline ze_fabric_vertex_handle_t toHandle() { return this; }
    ze_result_t edgeGet(ze_fabric_vertex_handle_t hVertexB,
                        uint32_t *pCount, ze_fabric_edge_handle_t *phEdges);
    std::map<FabricDeviceInterface::Type, std::unique_ptr<FabricDeviceInterface>> pFabricDeviceInterfaces;
    std::vector<FabricVertex *> subVertices = {};
    struct Device *device = nullptr;

  private:
    ze_fabric_vertex_exp_properties_t properties = {};
};

struct FabricEdge : _ze_fabric_edge_handle_t {

  public:
    virtual ~FabricEdge() = default;

    static void createEdgesFromVertices(const std::vector<FabricVertex *> &vertices, std::vector<FabricEdge *> &edges, std::vector<FabricEdge *> &indirectEdges);
    static FabricEdge *create(FabricVertex *vertexA, FabricVertex *vertexB, ze_fabric_edge_exp_properties_t &properties);
    ze_result_t getProperties(ze_fabric_edge_exp_properties_t *pEdgeProperties) const {
        *pEdgeProperties = properties;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t getVertices(ze_fabric_vertex_handle_t *phVertexA,
                            ze_fabric_vertex_handle_t *phVertexB) {
        *phVertexA = vertexA->toHandle();
        *phVertexB = vertexB->toHandle();
        return ZE_RESULT_SUCCESS;
    }

    static FabricEdge *fromHandle(ze_fabric_edge_handle_t handle) { return static_cast<FabricEdge *>(handle); }
    inline ze_fabric_edge_handle_t toHandle() { return this; }

    ze_fabric_edge_exp_properties_t properties = {};
    FabricVertex *vertexA = nullptr;
    FabricVertex *vertexB = nullptr;
};

} // namespace L0
