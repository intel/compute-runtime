/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

extern "C" {

typedef struct _ze_gpu_driver_dditable_t {
    ze_dditable_t ddiTable;

    ze_dditable_t coreDdiTable;
    ze_dditable_t tracingDdiTable;
    zet_dditable_t toolsDdiTable;
    zes_dditable_t sysmanDdiTable;

    ze_api_version_t version = ZE_API_VERSION_1_0;

    bool enableTracing;
} ze_gpu_driver_dditable_t;

extern ze_gpu_driver_dditable_t driverDdiTable;

} // extern "C"

template <typename FuncType>
inline void fillDdiEntry(FuncType &entry, FuncType function, ze_api_version_t loaderVersion, ze_api_version_t requiredVersion) {
    if (loaderVersion >= requiredVersion) {
        entry = function;
    }
}
