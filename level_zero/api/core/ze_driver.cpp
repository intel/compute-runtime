/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeInit(
    ze_init_flags_t flags) {
    return L0::init(flags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGet(
    uint32_t *pCount,
    ze_driver_handle_t *phDrivers) {
    return L0::driverHandleGet(pCount, phDrivers);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetProperties(
    ze_driver_handle_t hDriver,
    ze_driver_properties_t *pProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getProperties(pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetApiVersion(
    ze_driver_handle_t hDriver,
    ze_api_version_t *version) {
    return L0::DriverHandle::fromHandle(hDriver)->getApiVersion(version);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetIpcProperties(
    ze_driver_handle_t hDriver,
    ze_driver_ipc_properties_t *pIPCProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getIPCProperties(pIPCProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetExtensionProperties(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_driver_extension_properties_t *pExtensionProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getExtensionProperties(pCount, pExtensionProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetExtensionFunctionAddress(
    ze_driver_handle_t hDriver,
    const char *name,
    void **ppFunctionAddress) {
    return L0::DriverHandle::fromHandle(hDriver)->getExtensionFunctionAddress(name, ppFunctionAddress);
}