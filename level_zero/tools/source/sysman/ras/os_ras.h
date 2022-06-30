/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <set>

namespace L0 {

struct OsSysman;
class OsRas {
  public:
    virtual ze_result_t osRasGetProperties(zes_ras_properties_t &properties) = 0;
    virtual ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) = 0;
    virtual ze_result_t osRasGetConfig(zes_ras_config_t *config) = 0;
    virtual ze_result_t osRasSetConfig(const zes_ras_config_t *config) = 0;
    static OsRas *create(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle);
    virtual ~OsRas() = default;
};

} // namespace L0
