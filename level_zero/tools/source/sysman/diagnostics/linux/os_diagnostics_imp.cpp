/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"

#include "shared/source/helpers/string.h"

namespace L0 {

bool LinuxDiagnosticsImp::isDiagnosticsSupported(void) {
    if (pFwInterface != nullptr) {
        isFWInitalized = ((ZE_RESULT_SUCCESS == pFwInterface->fwDeviceInit()) ? true : false);
        return this->isFWInitalized;
    }
    return false;
}

void LinuxDiagnosticsImp::osGetDiagProperties(zes_diag_properties_t *pProperties) {
    return;
}

LinuxDiagnosticsImp::LinuxDiagnosticsImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsDiagnostics> OsDiagnostics::create(OsSysman *pOsSysman) {
    std::unique_ptr<LinuxDiagnosticsImp> pLinuxDiagnosticsImp = std::make_unique<LinuxDiagnosticsImp>(pOsSysman);
    return pLinuxDiagnosticsImp;
}

} // namespace L0
