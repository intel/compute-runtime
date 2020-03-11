/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeInit(
    ze_init_flag_t flags) {
    return L0::init(flags);
}

__zedllexport ze_result_t __zecall
zeDriverGet(
    uint32_t *pCount,
    ze_driver_handle_t *phDrivers) {
    return L0::driverHandleGet(pCount, phDrivers);
}

__zedllexport ze_result_t __zecall
zeDriverGetProperties(
    ze_driver_handle_t hDriver,
    ze_driver_properties_t *pProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zeDriverGetApiVersion(
    ze_driver_handle_t hDriver,
    ze_api_version_t *version) {
    return L0::DriverHandle::fromHandle(hDriver)->getApiVersion(version);
}

__zedllexport ze_result_t __zecall
zeDriverGetIPCProperties(
    ze_driver_handle_t hDriver,
    ze_driver_ipc_properties_t *pIPCProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getIPCProperties(pIPCProperties);
}

__zedllexport ze_result_t __zecall
zeDriverGetExtensionFunctionAddress(
    ze_driver_handle_t hDriver,
    const char *pFuncName,
    void **pfunc) {
    return L0::DriverHandle::fromHandle(hDriver)->getExtensionFunctionAddress(pFuncName, pfunc);
}

} // extern "C"
