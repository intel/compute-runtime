/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

extern "C" {

typedef struct _ze_gpu_driver_dditable_t {
    ze_dditable_t ddiTable;

    ze_dditable_t core_ddiTable;
    ze_dditable_t tracing_ddiTable;
    zet_dditable_t tools_ddiTable;

    ze_api_version_t version = ZE_API_VERSION_1_0;

    bool enableTracing;

    void *driverLibrary;

} ze_gpu_driver_dditable_t;

extern ze_gpu_driver_dditable_t driver_ddiTable;

} // extern "C"
