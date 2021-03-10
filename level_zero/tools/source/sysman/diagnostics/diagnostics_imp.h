/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/sysman/diagnostics/diagnostics.h"
#include "level_zero/tools/source/sysman/diagnostics/os_diagnostics.h"
#include <level_zero/zes_api.h>

namespace L0 {

class OsDiagnostics;

class DiagnosticsImp : public Diagnostics, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t diagnosticsGetProperties(zes_diag_properties_t *pProperties) override;
    DiagnosticsImp() = default;
    DiagnosticsImp(OsSysman *pOsSysman);
    ~DiagnosticsImp() override;
    std::unique_ptr<OsDiagnostics> pOsDiagnostics = nullptr;

    void init();
};

} // namespace L0
