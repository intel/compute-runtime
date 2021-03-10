/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/diagnostics/os_diagnostics.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {
class WddmDiagnosticsImp : public OsDiagnostics {
  public:
    bool isDiagnosticsSupported(void) override;
    void osGetDiagProperties(zes_diag_properties_t *pProperties) override;
};

} // namespace L0
