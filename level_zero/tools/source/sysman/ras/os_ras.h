/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

namespace L0 {

struct OsSysman;
class OsRas {
  public:
    virtual ze_result_t getCounterValues(zet_ras_details_t *pDetails) = 0;
    static OsRas *create(OsSysman *pOsSysman);
    virtual ~OsRas() = default;
};

} // namespace L0
