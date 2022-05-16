/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeSamplerCreateTracing(ze_context_handle_t hContext,
                       ze_device_handle_t hDevice,
                       const ze_sampler_desc_t *desc,
                       ze_sampler_handle_t *phSampler);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeSamplerDestroyTracing(ze_sampler_handle_t hSampler);
}
