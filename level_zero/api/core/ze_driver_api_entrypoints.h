/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t ZE_APICALL zeInit(
    ze_init_flags_t flags);

ze_result_t ZE_APICALL zeInitDrivers(
    uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc);

ze_result_t ZE_APICALL zeDriverGet(
    uint32_t *pCount,
    ze_driver_handle_t *phDrivers);

ze_result_t ZE_APICALL zeDriverGetProperties(
    ze_driver_handle_t hDriver,
    ze_driver_properties_t *pProperties);

ze_result_t ZE_APICALL zeDriverGetApiVersion(
    ze_driver_handle_t hDriver,
    ze_api_version_t *version);

ze_result_t ZE_APICALL zeDriverGetIpcProperties(
    ze_driver_handle_t hDriver,
    ze_driver_ipc_properties_t *pIPCProperties);

ze_result_t ZE_APICALL zeDriverGetLastErrorDescription(
    ze_driver_handle_t hDriver,
    const char **ppString);

ze_result_t ZE_APICALL zeDriverGetExtensionProperties(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_driver_extension_properties_t *pExtensionProperties);

ze_result_t ZE_APICALL zeDriverGetExtensionFunctionAddress(
    ze_driver_handle_t hDriver,
    const char *name,
    void **ppFunctionAddress);

ze_result_t ZE_APICALL zeDriverRTASFormatCompatibilityCheckExt(ze_driver_handle_t hDriver,
                                                               ze_rtas_format_ext_t rtasFormatA,
                                                               ze_rtas_format_ext_t rtasFormatB);

ze_context_handle_t ZE_APICALL zeDriverGetDefaultContext(
    ze_driver_handle_t hDriver);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeInit(
    ze_init_flags_t flags);

ZE_APIEXPORT ze_result_t ZE_APICALL zeInitDrivers(
    uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGet(
    uint32_t *pCount,
    ze_driver_handle_t *phDrivers);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetApiVersion(
    ze_driver_handle_t hDriver,
    ze_api_version_t *version);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetProperties(
    ze_driver_handle_t hDriver,
    ze_driver_properties_t *pDriverProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetIpcProperties(
    ze_driver_handle_t hDriver,
    ze_driver_ipc_properties_t *pIpcProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetLastErrorDescription(
    ze_driver_handle_t hDriver,
    const char **ppString);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetExtensionProperties(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_driver_extension_properties_t *pExtensionProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetExtensionFunctionAddress(
    ze_driver_handle_t hDriver,
    const char *name,
    void **ppFunctionAddress);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverRTASFormatCompatibilityCheckExt(
    ze_driver_handle_t hDriver,
    ze_rtas_format_ext_t rtasFormatA,
    ze_rtas_format_ext_t rtasFormatB);

ZE_APIEXPORT ze_context_handle_t ZE_APICALL zeDriverGetDefaultContext(
    ze_driver_handle_t hDriver);

} // extern "C"
