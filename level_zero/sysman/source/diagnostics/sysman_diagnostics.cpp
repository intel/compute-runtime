/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/diagnostics/sysman_diagnostics.h"

#include "shared/source/helpers/basic_math.h"

namespace L0 {
namespace Sysman {

DiagnosticsHandleContext::~DiagnosticsHandleContext() {
    releaseDiagnosticsHandles();
}

void DiagnosticsHandleContext::releaseDiagnosticsHandles() {
    for (Diagnostics *pDiagnostics : handleList) {
        delete pDiagnostics;
    }
    handleList.clear();
}

void DiagnosticsHandleContext::init() {
}

ze_result_t DiagnosticsHandleContext::diagnosticsGet(uint32_t *pCount, zes_diag_handle_t *phDiagnostics) {
    std::call_once(initDiagnosticsOnce, [this]() {
        this->init();
        this->diagnosticsInitDone = true;
    });
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

} // namespace Sysman
} // namespace L0
