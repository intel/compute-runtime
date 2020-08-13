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

class OsFirmware {
  public:
    virtual bool isFirmwareSupported(void) = 0;

    static OsFirmware *create(OsSysman *pOsSysman);
    virtual ~OsFirmware() {}
};

} // namespace L0
