/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <memory>

namespace L0 {

struct Device;
struct FabricVertex;

class FabricDeviceInterface {

  public:
    enum class Type {
        Undefined,
        Iaf,
        Mdfi
    };
    virtual ~FabricDeviceInterface(){};
    virtual ze_result_t enumerate() = 0;
    static std::unique_ptr<FabricDeviceInterface> createFabricDeviceInterfaceIaf(const FabricVertex *fabricVertex);
    static std::unique_ptr<FabricDeviceInterface> createFabricDeviceInterfaceMdfi(const FabricVertex *fabricVertex);
    virtual bool getEdgeProperty(FabricVertex *neighborVertex, ze_fabric_edge_exp_properties_t &edgeProperty) = 0;
};

class FabricDeviceMdfi : public FabricDeviceInterface {
  public:
    FabricDeviceMdfi(Device *device) : device(device) {}
    ~FabricDeviceMdfi() override = default;
    ze_result_t enumerate() override {
        return ZE_RESULT_SUCCESS;
    }
    bool getEdgeProperty(FabricVertex *neighborVertex, ze_fabric_edge_exp_properties_t &edgeProperty) override;

  protected:
    Device *device = nullptr;
};

} // namespace L0
