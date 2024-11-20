/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/include/ze_intel_gpu.h"

ze_result_t ZE_APICALL
zeIntelDeviceImportExternalSemaphoreExp(
    ze_device_handle_t device,
    const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc,
    ze_intel_external_semaphore_exp_handle_t *phSemaphore) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ZE_APICALL
zeIntelDeviceReleaseExternalSemaphoreExp(
    ze_intel_external_semaphore_exp_handle_t hSemaphore) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}