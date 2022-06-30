/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <string>
#include <vector>

namespace L0 {

class OsDiagnostics {
  public:
    virtual void osGetDiagProperties(zes_diag_properties_t *pProperties) = 0;
    virtual ze_result_t osGetDiagTests(uint32_t *pCount, zes_diag_test_t *pTests) = 0;
    virtual ze_result_t osRunDiagTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) = 0;
    static std::unique_ptr<OsDiagnostics> create(OsSysman *pOsSysman, const std::string &diagTests);
    static void getSupportedDiagTestsFromFW(void *pOsSysman, std::vector<std::string> &supportedDiagTests);
    virtual ~OsDiagnostics() {}
};

} // namespace L0
