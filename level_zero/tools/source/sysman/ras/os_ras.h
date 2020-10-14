/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <vector>

namespace L0 {

struct OsSysman;
class OsRas {
  public:
    virtual ze_result_t osRasGetProperties(zes_ras_properties_t &properties) = 0;
    virtual ze_result_t osRasGetState(zes_ras_state_t &state) = 0;
    static OsRas *create(OsSysman *pOsSysman, zes_ras_error_type_t type);
    static ze_result_t getSupportedRasErrorTypes(std::vector<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman);
    virtual ~OsRas() = default;
};

} // namespace L0
