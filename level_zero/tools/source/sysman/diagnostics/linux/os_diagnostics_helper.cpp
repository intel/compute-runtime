/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"

namespace L0 {

void OsDiagnostics::getSupportedDiagTestsFromFW(void *pOsSysman, std::vector<std::string> &supportedDiagTests) {
}

ze_result_t LinuxDiagnosticsImp::osRunDiagTestsinFW(zes_diag_result_t *pResult) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
} // namespace L0