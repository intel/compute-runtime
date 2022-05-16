/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetTracing(uint32_t *pCount,
                   ze_driver_handle_t *phDrivers);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetPropertiesTracing(ze_driver_handle_t hDriver,
                             ze_driver_properties_t *properties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetApiVersionTracing(ze_driver_handle_t hDrivers,
                             ze_api_version_t *version);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetIpcPropertiesTracing(ze_driver_handle_t hDriver,
                                ze_driver_ipc_properties_t *pIpcProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetExtensionPropertiesTracing(ze_driver_handle_t hDriver,
                                      uint32_t *pCount,
                                      ze_driver_extension_properties_t *pExtensionProperties);
}
