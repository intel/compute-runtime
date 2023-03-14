/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/diagnostics/diagnostics_imp.h"

namespace L0 {
class OsDiagnostics;
DiagnosticsHandleContext::~DiagnosticsHandleContext() {
    releaseDiagnosticsHandles();
}

void DiagnosticsHandleContext::releaseDiagnosticsHandles() {
    handleList.clear();
}
void DiagnosticsHandleContext::createHandle(const std::string &diagTests) {
    std::unique_ptr<Diagnostics> pDiagnostics = std::make_unique<DiagnosticsImp>(pOsSysman, diagTests);
    handleList.push_back(std::move(pDiagnostics));
}

void DiagnosticsHandleContext::init() {
    OsDiagnostics::getSupportedDiagTestsFromFW(pOsSysman, supportedDiagTests);
    for (const std::string &diagTests : supportedDiagTests) {
        createHandle(diagTests);
    }
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
} // namespace L0
