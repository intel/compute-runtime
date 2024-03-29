/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/fabric/fabric_device_interface.h"
#include <level_zero/ze_api.h>

namespace L0 {

class FabricDeviceIaf : public FabricDeviceInterface {

  public:
    FabricDeviceIaf() = default;
    ~FabricDeviceIaf() override = default;
    ze_result_t enumerate() override {
        return ZE_RESULT_SUCCESS;
    }
    bool getEdgeProperty(FabricVertex *neighborVertex, ze_fabric_edge_exp_properties_t &edgeProperty) override {
        return false;
    }
};

} // namespace L0
