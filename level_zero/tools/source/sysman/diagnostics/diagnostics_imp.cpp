/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "diagnostics_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "os_diagnostics.h"

namespace L0 {

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

} // namespace L0
