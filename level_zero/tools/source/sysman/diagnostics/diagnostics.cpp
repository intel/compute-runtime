/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/diagnostics/diagnostics_imp.h"

namespace L0 {
class OsDiagnostics;
DiagnosticsHandleContext::~DiagnosticsHandleContext() {
    for (Diagnostics *pDiagnostics : handleList) {
        delete pDiagnostics;
    }
    handleList.clear();
}

void DiagnosticsHandleContext::createHandle() {
    Diagnostics *pDiagnostics = new DiagnosticsImp(pOsSysman);
    if (pDiagnostics->isDiagnosticsEnabled == true) {
        handleList.push_back(pDiagnostics);
    } else {
        delete pDiagnostics;
    }
}

void DiagnosticsHandleContext::init() {
    createHandle();
}

ze_result_t DiagnosticsHandleContext::diagnosticsGet(uint32_t *pCount, zes_diag_handle_t *phDiagnostics) {
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phDiagnostics) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phDiagnostics[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}
} // namespace L0
