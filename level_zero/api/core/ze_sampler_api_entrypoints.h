/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t ZE_APICALL zeSamplerCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_sampler_desc_t *desc,
    ze_sampler_handle_t *phSampler);

ze_result_t ZE_APICALL zeSamplerDestroy(
    ze_sampler_handle_t hSampler);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeSamplerCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_sampler_desc_t *desc,
    ze_sampler_handle_t *phSampler);

ZE_APIEXPORT ze_result_t ZE_APICALL zeSamplerDestroy(
    ze_sampler_handle_t hSampler);

} // extern "C"
