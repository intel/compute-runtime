/*
 * Copyright (C) 2021 Intel Corporation
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
    for (Diagnostics *pDiagnostics : handleList) {
        delete pDiagnostics;
    }
    handleList.clear();
}
void DiagnosticsHandleContext::createHandle(ze_device_handle_t deviceHandle, const std::string &diagTests) {
    Diagnostics *pDiagnostics = new DiagnosticsImp(pOsSysman, diagTests, deviceHandle);
    handleList.push_back(pDiagnostics);
}

void DiagnosticsHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    OsDiagnostics::getSupportedDiagTestsFromFW(pOsSysman, supportedDiagTests);
    for (const auto &deviceHandle : deviceHandles) {
        for (const std::string &diagTests : supportedDiagTests) {
            createHandle(deviceHandle, diagTests);
        }
    }
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
