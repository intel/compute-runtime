/*
 * Copyright (C) 2021-2022 Intel Corporation
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
    ze_result_t diagnosticsGetTests(uint32_t *pCount, zes_diag_test_t *pTests) override;
    ze_result_t diagnosticsRunTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) override;
    DiagnosticsImp() = default;
    DiagnosticsImp(OsSysman *pOsSysman, const std::string &initalizedDiagTest);
    ~DiagnosticsImp() override;
    std::unique_ptr<OsDiagnostics> pOsDiagnostics = nullptr;
};

} // namespace L0
