/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/ze_intel_gpu.h"

#include "driver_version.h"

#include <string>

namespace L0 {

ze_result_t ZE_APICALL
zexDriverImportExternalPointer(
    ze_driver_handle_t hDriver,
    void *ptr,
    size_t size) {
    return L0::DriverHandle::fromHandle(toInternalType(hDriver))->importExternalPointer(ptr, size);
}

ze_result_t ZE_APICALL
zexDriverReleaseImportedPointer(
    ze_driver_handle_t hDriver,
    void *ptr) {
    return L0::DriverHandle::fromHandle(toInternalType(hDriver))->releaseImportedPointer(ptr);
}

ze_result_t ZE_APICALL
zexDriverGetHostPointerBaseAddress(
    ze_driver_handle_t hDriver,
    void *ptr,
    void **baseAddress) {
    return L0::DriverHandle::fromHandle(toInternalType(hDriver))->getHostPointerBaseAddress(ptr, baseAddress);
}

ze_result_t ZE_APICALL
zeIntelGetDriverVersionString(
    ze_driver_handle_t hDriver,
    char *pDriverVersion,
    size_t *pVersionSize) {
    ze_api_version_t apiVersion;
    L0::DriverHandle::fromHandle(toInternalType(hDriver))->getApiVersion(&apiVersion);
    std::string driverVersionString = std::to_string(ZE_MAJOR_VERSION(apiVersion)) + "." + std::to_string(ZE_MINOR_VERSION(apiVersion)) + "." + std::to_string(NEO_VERSION_BUILD);
    if (NEO_VERSION_HOTFIX > 0) {
        driverVersionString.append("+");
        driverVersionString.append(std::to_string(NEO_VERSION_HOTFIX));
    }
    if (*pVersionSize == 0) {
        *pVersionSize = strlen(driverVersionString.c_str());
        return ZE_RESULT_SUCCESS;
    }
    driverVersionString.copy(pDriverVersion, *pVersionSize, 0);
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeIntelGetDriverVersionString(
    ze_driver_handle_t hDriver,
    char *pDriverVersion,
    size_t *pVersionSize) {
    return L0::zeIntelGetDriverVersionString(hDriver, pDriverVersion, pVersionSize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexDriverImportExternalPointer(
    ze_driver_handle_t hDriver,
    void *ptr,
    size_t size) {
    return L0::zexDriverImportExternalPointer(hDriver, ptr, size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexDriverReleaseImportedPointer(
    ze_driver_handle_t hDriver,
    void *ptr) {
    return L0::zexDriverReleaseImportedPointer(hDriver, ptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexDriverGetHostPointerBaseAddress(
    ze_driver_handle_t hDriver,
    void *ptr,
    void **baseAddress) {
    return L0::zexDriverGetHostPointerBaseAddress(hDriver, ptr, baseAddress);
}
} // extern "C"
