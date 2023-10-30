/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/device/os_sysman.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

class OsEvents {
  public:
    static OsEvents *create(OsSysman *pOsSysman);
    virtual bool eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) = 0;
    virtual ze_result_t eventRegister(zes_event_type_flags_t events) = 0;
    virtual ~OsEvents() {}
};

} // namespace Sysman
} // namespace L0
