/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/api/diagnostics/sysman_diagnostics.h"
#include "level_zero/sysman/source/api/diagnostics/sysman_os_diagnostics.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

class DiagnosticsImp : public Diagnostics, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t diagnosticsGetProperties(zes_diag_properties_t *pProperties) override;
    ze_result_t diagnosticsGetTests(uint32_t *pCount, zes_diag_test_t *pTests) override;
    ze_result_t diagnosticsRunTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) override;
    DiagnosticsImp() = default;
    DiagnosticsImp(OsSysman *pOsSysman, const std::string &initializedDiagTest);
    ~DiagnosticsImp() override;
    std::unique_ptr<OsDiagnostics> pOsDiagnostics = nullptr;
};

} // namespace Sysman
} // namespace L0
