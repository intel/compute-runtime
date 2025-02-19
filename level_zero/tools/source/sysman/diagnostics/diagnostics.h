/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace L0 {

struct OsSysman;

class Diagnostics : _zes_diag_handle_t {
  public:
    virtual ~Diagnostics() = default;
    virtual ze_result_t diagnosticsGetProperties(zes_diag_properties_t *pProperties) = 0;
    virtual ze_result_t diagnosticsGetTests(uint32_t *pCount, zes_diag_test_t *pTests) = 0;
    virtual ze_result_t diagnosticsRunTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) = 0;
    inline zes_diag_handle_t toHandle() { return this; }

    static Diagnostics *fromHandle(zes_diag_handle_t handle) {
        return static_cast<Diagnostics *>(handle);
    }
};

struct DiagnosticsHandleContext {
    DiagnosticsHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    void releaseDiagnosticsHandles();
    MOCKABLE_VIRTUAL ~DiagnosticsHandleContext();

    MOCKABLE_VIRTUAL void init();

    ze_result_t diagnosticsGet(uint32_t *pCount, zes_diag_handle_t *phDiagnostics);
    std::vector<std::string> supportedDiagTests = {};
    OsSysman *pOsSysman = nullptr;
    std::vector<std::unique_ptr<Diagnostics>> handleList = {};
    bool isDiagnosticsInitDone() {
        return diagnosticsInitDone;
    }

  private:
    void createHandle(const std::string &diagTests);
    std::once_flag initDiagnosticsOnce;
    bool diagnosticsInitDone = false;
};

} // namespace L0
