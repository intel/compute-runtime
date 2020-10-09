/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

namespace L0 {

class OsEvents {
  public:
    static OsEvents *create(OsSysman *pOsSysman);
    virtual bool isResetRequired(zes_event_type_flags_t &pEvent) = 0;
    virtual ~OsEvents() {}
};

} // namespace L0
