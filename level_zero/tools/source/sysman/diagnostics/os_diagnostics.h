/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <string>
#include <vector>

namespace L0 {

class OsDiagnostics {
  public:
    virtual bool isDiagnosticsSupported(void) = 0;
    virtual void osGetDiagProperties(zes_diag_properties_t *pProperties) = 0;
    static std::unique_ptr<OsDiagnostics> create(OsSysman *pOsSysman);
    virtual ~OsDiagnostics() {}
};

} // namespace L0
