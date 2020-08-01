/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGet_Tracing(uint32_t *pCount,
                    ze_driver_handle_t *phDrivers);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetProperties_Tracing(ze_driver_handle_t hDriver,
                              ze_driver_properties_t *properties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetApiVersion_Tracing(ze_driver_handle_t hDrivers,
                              ze_api_version_t *version);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetIPCProperties_Tracing(ze_driver_handle_t hDriver,
                                 ze_driver_ipc_properties_t *pIPCProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetExtensionFunctionAddress_Tracing(ze_driver_handle_t hDriver,
                                            const char *pFuncName,
                                            void **pfunc);
}
