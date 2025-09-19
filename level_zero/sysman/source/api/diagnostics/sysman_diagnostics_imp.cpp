/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/diagnostics/sysman_diagnostics_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/diagnostics/sysman_os_diagnostics.h"

namespace L0 {
namespace Sysman {

ze_result_t DiagnosticsImp::diagnosticsGetProperties(zes_diag_properties_t *pProperties) {
    pOsDiagnostics->osGetDiagProperties(pProperties);
    return ZE_RESULT_SUCCESS;
}

ze_result_t DiagnosticsImp::diagnosticsGetTests(uint32_t *pCount, zes_diag_test_t *pTests) {
    return pOsDiagnostics->osGetDiagTests(pCount, pTests);
}

ze_result_t DiagnosticsImp::diagnosticsRunTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) {
    return pOsDiagnostics->osRunDiagTests(start, end, pResult);
}

DiagnosticsImp::DiagnosticsImp(OsSysman *pOsSysman, const std::string &initializedDiagTest) {
    pOsDiagnostics = OsDiagnostics::create(pOsSysman, initializedDiagTest);
    UNRECOVERABLE_IF(nullptr == pOsDiagnostics);
}

DiagnosticsImp::~DiagnosticsImp() {
}

} // namespace Sysman
} // namespace L0
