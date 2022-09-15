/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/fabric/fabric_device_interface.h"
#include <level_zero/ze_api.h>

namespace L0 {

class FabricDeviceIafStub : public FabricDeviceInterface {

  public:
    FabricDeviceIafStub() = default;
    ~FabricDeviceIafStub() override = default;
    ze_result_t enumerate() override {
        return ZE_RESULT_SUCCESS;
    }
};

} // namespace L0