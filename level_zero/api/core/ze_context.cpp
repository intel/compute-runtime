/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextCreate(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    ze_context_handle_t *phContext) {
    return L0::DriverHandle::fromHandle(hDriver)->createContext(desc, phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextDestroy(ze_context_handle_t hContext) {
    return L0::Context::fromHandle(hContext)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextGetStatus(ze_context_handle_t hContext) {
    return L0::Context::fromHandle(hContext)->getStatus();
}

} // extern "C"
