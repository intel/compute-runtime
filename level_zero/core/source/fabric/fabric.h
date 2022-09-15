/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <memory>
#include <vector>

struct _ze_fabric_vertex_handle_t {};

namespace L0 {
struct Device;
class FabricDeviceInterface;

struct FabricVertex : _ze_fabric_vertex_handle_t {

  public:
    static FabricVertex *createFromDevice(Device *device);
    virtual ~FabricVertex();
    ze_result_t getSubVertices(uint32_t *pCount, ze_fabric_vertex_handle_t *phSubvertices);
    ze_result_t getProperties(ze_fabric_vertex_exp_properties_t *pVertexProperties) const;
    ze_result_t getDevice(ze_device_handle_t *phDevice) const;
    static FabricVertex *fromHandle(ze_fabric_vertex_handle_t handle) { return static_cast<FabricVertex *>(handle); }
    inline ze_fabric_vertex_handle_t toHandle() { return this; }

    std::unique_ptr<FabricDeviceInterface> pFabricDeviceInterface;

  private:
    struct Device *device = nullptr;
    ze_fabric_vertex_exp_properties_t properties = {};

    std::vector<FabricVertex *> subVertices = {};
};

} // namespace L0
