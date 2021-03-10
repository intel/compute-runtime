/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "diagnostics_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "os_diagnostics.h"

#include <cmath>

namespace L0 {

ze_result_t DiagnosticsImp::diagnosticsGetProperties(zes_diag_properties_t *pProperties) {
    pOsDiagnostics->osGetDiagProperties(pProperties);
    return ZE_RESULT_SUCCESS;
}

void DiagnosticsImp::init() {
    this->isDiagnosticsEnabled = pOsDiagnostics->isDiagnosticsSupported();
}

DiagnosticsImp::DiagnosticsImp(OsSysman *pOsSysman) {
    pOsDiagnostics = OsDiagnostics::create(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pOsDiagnostics);
    init();
}

DiagnosticsImp::~DiagnosticsImp() {
}

} // namespace L0
