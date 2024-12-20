/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/ze_intel_gpu.h"

ze_result_t ZE_APICALL
zeIntelDeviceImportExternalSemaphoreExp(
    ze_device_handle_t device,
    const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc,
    ze_intel_external_semaphore_exp_handle_t *phSemaphore) {
    return L0::ExternalSemaphore::importExternalSemaphore(device, semaphoreDesc, phSemaphore);
}

ze_result_t ZE_APICALL
zeIntelDeviceReleaseExternalSemaphoreExp(
    ze_intel_external_semaphore_exp_handle_t hSemaphore) {
    return L0::ExternalSemaphoreImp::fromHandle(hSemaphore)->releaseExternalSemaphore();
}
