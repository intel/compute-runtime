/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

#include "sysman/os_sysman.h"

namespace L0 {

class OsStandby {
  public:
    virtual ze_result_t getMode(zet_standby_promo_mode_t &mode) = 0;
    virtual ze_result_t setMode(zet_standby_promo_mode_t mode) = 0;

    static OsStandby *create(OsSysman *pOsSysman);
    virtual ~OsStandby() {}
};

} // namespace L0
