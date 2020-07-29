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
class OsRas {
  public:
    virtual bool isRasSupported(void) = 0;
    virtual void setRasErrorType(zes_ras_error_type_t type) = 0;
    static OsRas *create(OsSysman *pOsSysman);
    virtual ~OsRas() = default;
};

} // namespace L0
