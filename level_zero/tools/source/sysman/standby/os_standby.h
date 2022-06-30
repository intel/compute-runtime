/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

namespace L0 {

class OsStandby {
  public:
    virtual ze_result_t getMode(zes_standby_promo_mode_t &mode) = 0;
    virtual ze_result_t setMode(zes_standby_promo_mode_t mode) = 0;
    virtual ze_result_t osStandbyGetProperties(zes_standby_properties_t &properties) = 0;

    virtual bool isStandbySupported(void) = 0;

    static OsStandby *create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ~OsStandby() {}
};

} // namespace L0
