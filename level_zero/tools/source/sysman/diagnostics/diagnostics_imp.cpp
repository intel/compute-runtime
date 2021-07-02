/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "diagnostics_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "os_diagnostics.h"

#include <cmath>

namespace L0 {

ze_result_t DiagnosticsImp::diagnosticsGetProperties(zes_diag_properties_t *pProperties) {
    pOsDiagnostics->osGetDiagProperties(pProperties);
    return ZE_RESULT_SUCCESS;
}

ze_result_t DiagnosticsImp::diagnosticsGetTests(uint32_t *pCount, zes_diag_test_t *pTests) {
    return pOsDiagnostics->osGetDiagTests(pCount, pTests);
}

ze_result_t DiagnosticsImp::diagnosticsRunTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) {
    return pOsDiagnostics->osRunDiagTests(start, end, pResult);
}

DiagnosticsImp::DiagnosticsImp(OsSysman *pOsSysman, const std::string &initalizedDiagTest, ze_device_handle_t handle) : deviceHandle(handle) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
    pOsDiagnostics = OsDiagnostics::create(pOsSysman, initalizedDiagTest, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
    UNRECOVERABLE_IF(nullptr == pOsDiagnostics);
}

DiagnosticsImp::~DiagnosticsImp() {
}

} // namespace L0
