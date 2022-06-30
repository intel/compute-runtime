/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/device/device.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <string>
#include <vector>

struct _zes_diag_handle_t {
    virtual ~_zes_diag_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Diagnostics : _zes_diag_handle_t {
  public:
    ~Diagnostics() override {}
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
    std::vector<Diagnostics *> handleList = {};

  private:
    void createHandle(const std::string &diagTests);
    std::once_flag initDiagnosticsOnce;
};

} // namespace L0
