/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <string>
#include <vector>

struct _zes_diag_handle_t {
    virtual ~_zes_diag_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Diagnostics : _zes_diag_handle_t {
  public:
    virtual ~Diagnostics() {}
    virtual ze_result_t diagnosticsGetProperties(zes_diag_properties_t *pProperties) = 0;
    inline zes_diag_handle_t toHandle() { return this; }

    static Diagnostics *fromHandle(zes_diag_handle_t handle) {
        return static_cast<Diagnostics *>(handle);
    }
    bool isDiagnosticsEnabled = false;
};

struct DiagnosticsHandleContext {
    DiagnosticsHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~DiagnosticsHandleContext();

    void init();

    ze_result_t diagnosticsGet(uint32_t *pCount, zes_diag_handle_t *phDiagnostics);

    OsSysman *pOsSysman = nullptr;
    std::vector<Diagnostics *> handleList = {};

  private:
    void createHandle();
};

} // namespace L0
