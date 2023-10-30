/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/diagnostics/sysman_diagnostics.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/diagnostics/sysman_diagnostics_imp.h"
#include "level_zero/sysman/source/api/diagnostics/sysman_os_diagnostics.h"
#include "level_zero/sysman/source/device/os_sysman.h"

namespace L0 {
namespace Sysman {
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

} // namespace Sysman
} // namespace L0
