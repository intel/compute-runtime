/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/zer_api.h>
#include <level_zero/zer_ddi.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zerGetGlobalProcAddrTable(
    ze_api_version_t version,
    zer_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.runtime.version) != ZE_MAJOR_VERSION(version)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetLastErrorDescription, L0::globalDriverDispatch.runtimeGlobal.pfnGetLastErrorDescription, version, ZE_API_VERSION_1_14);
    fillDdiEntry(pDdiTable->pfnTranslateDeviceHandleToIdentifier, L0::globalDriverDispatch.runtimeGlobal.pfnTranslateDeviceHandleToIdentifier, version, ZE_API_VERSION_1_14);
    fillDdiEntry(pDdiTable->pfnTranslateIdentifierToDeviceHandle, L0::globalDriverDispatch.runtimeGlobal.pfnTranslateIdentifierToDeviceHandle, version, ZE_API_VERSION_1_14);
    fillDdiEntry(pDdiTable->pfnGetDefaultContext, L0::globalDriverDispatch.runtimeGlobal.pfnGetDefaultContext, version, ZE_API_VERSION_1_14);
    return result;
}
