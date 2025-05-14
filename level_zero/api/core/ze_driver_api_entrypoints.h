/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeInit(
    ze_init_flags_t flags) {
    return L0::init(flags);
}

ze_result_t zeInitDrivers(
    uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc) {
    return L0::initDrivers(pCount, phDrivers, desc);
}

ze_result_t zeDriverGet(
    uint32_t *pCount,
    ze_driver_handle_t *phDrivers) {
    return L0::Driver::get()->driverHandleGet(pCount, phDrivers);
}

ze_result_t zeDriverGetProperties(
    ze_driver_handle_t hDriver,
    ze_driver_properties_t *pProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getProperties(pProperties);
}

ze_result_t zeDriverGetApiVersion(
    ze_driver_handle_t hDriver,
    ze_api_version_t *version) {
    return L0::DriverHandle::fromHandle(hDriver)->getApiVersion(version);
}

ze_result_t zeDriverGetIpcProperties(
    ze_driver_handle_t hDriver,
    ze_driver_ipc_properties_t *pIPCProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getIPCProperties(pIPCProperties);
}

ze_result_t zeDriverGetLastErrorDescription(
    ze_driver_handle_t hDriver,
    const char **ppString) {
    return L0::DriverHandle::fromHandle(hDriver)->getErrorDescription(ppString);
}

ze_result_t zeDriverGetExtensionProperties(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_driver_extension_properties_t *pExtensionProperties) {
    return L0::DriverHandle::fromHandle(hDriver)->getExtensionProperties(pCount, pExtensionProperties);
}

ze_result_t zeDriverGetExtensionFunctionAddress(
    ze_driver_handle_t hDriver,
    const char *name,
    void **ppFunctionAddress) {
    return L0::BaseDriver::fromHandle(hDriver)->getExtensionFunctionAddress(name, ppFunctionAddress);
}

ze_context_handle_t zeDriverGetDefaultContext(
    ze_driver_handle_t hDriver) {
    return L0::DriverHandle::fromHandle(hDriver)->getDefaultContext();
}

ze_context_handle_t zerDriverGetDefaultContext() {
    return L0::DriverHandle::fromHandle(L0::globalDriverHandles->front())->getDefaultContext();
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeInit(
    ze_init_flags_t flags) {
    return L0::zeInit(
        flags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeInitDrivers(
    uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc) {
    return L0::initDrivers(
        pCount, phDrivers, desc);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGet(
    uint32_t *pCount,
    ze_driver_handle_t *phDrivers) {
    return L0::zeDriverGet(
        pCount,
        phDrivers);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetApiVersion(
    ze_driver_handle_t hDriver,
    ze_api_version_t *version) {
    return L0::zeDriverGetApiVersion(
        hDriver,
        version);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetProperties(
    ze_driver_handle_t hDriver,
    ze_driver_properties_t *pDriverProperties) {
    return L0::zeDriverGetProperties(
        hDriver,
        pDriverProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetIpcProperties(
    ze_driver_handle_t hDriver,
    ze_driver_ipc_properties_t *pIpcProperties) {
    return L0::zeDriverGetIpcProperties(
        hDriver,
        pIpcProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetLastErrorDescription(
    ze_driver_handle_t hDriver,
    const char **ppString) {
    return L0::zeDriverGetLastErrorDescription(
        hDriver,
        ppString);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetExtensionProperties(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_driver_extension_properties_t *pExtensionProperties) {
    return L0::zeDriverGetExtensionProperties(
        hDriver,
        pCount,
        pExtensionProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDriverGetExtensionFunctionAddress(
    ze_driver_handle_t hDriver,
    const char *name,
    void **ppFunctionAddress) {
    return L0::zeDriverGetExtensionFunctionAddress(
        hDriver,
        name,
        ppFunctionAddress);
}

ze_context_handle_t ZE_APICALL zeDriverGetDefaultContext(
    ze_driver_handle_t hDriver) {
    return L0::zeDriverGetDefaultContext(hDriver);
}

ze_context_handle_t ZE_APICALL zerDriverGetDefaultContext() {
    return L0::zerDriverGetDefaultContext();
}
}
