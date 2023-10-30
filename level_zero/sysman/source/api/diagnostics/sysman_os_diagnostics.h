/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/device/os_sysman.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

class OsDiagnostics {
  public:
    virtual void osGetDiagProperties(zes_diag_properties_t *pProperties) = 0;
    virtual ze_result_t osGetDiagTests(uint32_t *pCount, zes_diag_test_t *pTests) = 0;
    virtual ze_result_t osRunDiagTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) = 0;
    static std::unique_ptr<OsDiagnostics> create(OsSysman *pOsSysman, const std::string &diagTests);
    static void getSupportedDiagTestsFromFW(void *pOsSysman, std::vector<std::string> &supportedDiagTests);
    virtual ~OsDiagnostics() {}
};

} // namespace Sysman
} // namespace L0
