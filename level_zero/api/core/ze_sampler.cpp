/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/sampler.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeSamplerCreate(
    ze_device_handle_t hDevice,
    const ze_sampler_desc_t *desc,
    ze_sampler_handle_t *phSampler) {
    return L0::Device::fromHandle(hDevice)->createSampler(desc, phSampler);
}

__zedllexport ze_result_t __zecall
zeSamplerDestroy(
    ze_sampler_handle_t hSampler) {
    return L0::Sampler::fromHandle(hSampler)->destroy();
}

} // extern "C"
