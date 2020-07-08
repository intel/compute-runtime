/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeDriverGet_Tracing(uint32_t *pCount,
                    ze_driver_handle_t *phDrivers);

__zedllexport ze_result_t __zecall
zeDriverGetProperties_Tracing(ze_driver_handle_t hDriver,
                              ze_driver_properties_t *properties);

__zedllexport ze_result_t __zecall
zeDriverGetApiVersion_Tracing(ze_driver_handle_t hDrivers,
                              ze_api_version_t *version);

__zedllexport ze_result_t __zecall
zeDriverGetIPCProperties_Tracing(ze_driver_handle_t hDriver,
                                 ze_driver_ipc_properties_t *pIPCProperties);

__zedllexport ze_result_t __zecall
zeDriverGetExtensionFunctionAddress_Tracing(ze_driver_handle_t hDriver,
                                            const char *pFuncName,
                                            void **pfunc);
}
