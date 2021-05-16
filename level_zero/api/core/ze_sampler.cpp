/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/sampler/sampler.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeSamplerCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_sampler_desc_t *desc,
    ze_sampler_handle_t *phSampler) {
    return L0::Context::fromHandle(hContext)->createSampler(hDevice, desc, phSampler);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeSamplerDestroy(
    ze_sampler_handle_t hSampler) {
    return L0::Sampler::fromHandle(hSampler)->destroy();
}
