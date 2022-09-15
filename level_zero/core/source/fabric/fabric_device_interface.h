/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/fabric/fabric.h"
#include <level_zero/ze_api.h>

#include <memory>

namespace L0 {

class FabricDeviceInterface {

  public:
    virtual ~FabricDeviceInterface(){};
    virtual ze_result_t enumerate() = 0;
    static std::unique_ptr<FabricDeviceInterface> createFabricDeviceInterface(const FabricVertex &fabricVertex);
};

} // namespace L0
