/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/diagnostics/sysman_os_diagnostics.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class WddmDiagnosticsImp : public OsDiagnostics {
  public:
    void osGetDiagProperties(zes_diag_properties_t *pProperties) override;
    ze_result_t osGetDiagTests(uint32_t *pCount, zes_diag_test_t *pTests) override;
    ze_result_t osRunDiagTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) override;
};

} // namespace Sysman
} // namespace L0
