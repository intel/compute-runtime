/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/sampler/sampler.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeSamplerCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_sampler_desc_t *desc,
    ze_sampler_handle_t *phSampler) {
    return L0::Context::fromHandle(hContext)->createSampler(hDevice, desc, phSampler);
}

ze_result_t zeSamplerDestroy(
    ze_sampler_handle_t hSampler) {
    return L0::Sampler::fromHandle(hSampler)->destroy();
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeSamplerCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_sampler_desc_t *desc,
    ze_sampler_handle_t *phSampler) {
    return L0::zeSamplerCreate(
        hContext,
        hDevice,
        desc,
        phSampler);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeSamplerDestroy(
    ze_sampler_handle_t hSampler) {
    return L0::zeSamplerDestroy(
        hSampler);
}
}
