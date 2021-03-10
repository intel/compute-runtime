/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/diagnostics/diagnostics_imp.h"
#include "sysman/diagnostics/os_diagnostics.h"
#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxDiagnosticsImp : public OsDiagnostics, NEO::NonCopyableOrMovableClass {
  public:
    bool isDiagnosticsSupported(void) override;
    void osGetDiagProperties(zes_diag_properties_t *pProperties) override;
    LinuxDiagnosticsImp() = default;
    LinuxDiagnosticsImp(OsSysman *pOsSysman);
    ~LinuxDiagnosticsImp() override = default;

  protected:
    FirmwareUtil *pFwInterface = nullptr;
    bool isFWInitalized = false;
};

} // namespace L0
