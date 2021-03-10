/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/diagnostics/windows/os_diagnostics_imp.h"

namespace L0 {

bool WddmDiagnosticsImp::isDiagnosticsSupported(void) {
    return false;
}

void WddmDiagnosticsImp::osGetDiagProperties(zes_diag_properties_t *pProperties){};

std::unique_ptr<OsDiagnostics> OsDiagnostics::create(OsSysman *pOsSysman) {
    std::unique_ptr<WddmDiagnosticsImp> pWddmDiagnosticsImp = std::make_unique<WddmDiagnosticsImp>();
    return pWddmDiagnosticsImp;
}

} // namespace L0
