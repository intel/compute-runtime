/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <set>

namespace L0 {
namespace Sysman {

struct OsSysman;
class OsRas {
  public:
    virtual ze_result_t osRasGetProperties(zes_ras_properties_t &properties) = 0;
    virtual ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) = 0;
    virtual ze_result_t osRasGetStateExp(uint32_t *pCount, zes_ras_state_exp_t *pState) = 0;
    virtual ze_result_t osRasGetConfig(zes_ras_config_t *config) = 0;
    virtual ze_result_t osRasSetConfig(const zes_ras_config_t *config) = 0;
    virtual ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) = 0;
    static OsRas *create(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_bool_t isSubDevice, uint32_t subDeviceId);
    virtual ~OsRas() = default;
};

} // namespace Sysman
} // namespace L0
