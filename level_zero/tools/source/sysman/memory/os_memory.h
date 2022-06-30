/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

namespace L0 {

struct OsSysman;
class OsMemory {
  public:
    virtual ze_result_t getProperties(zes_mem_properties_t *pProperties) = 0;
    virtual ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) = 0;
    virtual ze_result_t getState(zes_mem_state_t *pState) = 0;
    virtual bool isMemoryModuleSupported() = 0;
    static OsMemory *create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ~OsMemory() {}
};

} // namespace L0
