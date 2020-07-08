/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeSamplerCreate_Tracing(ze_device_handle_t hDevice,
                        const ze_sampler_desc_t *desc,
                        ze_sampler_handle_t *phSampler);

__zedllexport ze_result_t __zecall
zeSamplerDestroy_Tracing(ze_sampler_handle_t hSampler);
}
