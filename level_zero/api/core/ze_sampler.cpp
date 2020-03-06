/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/sampler.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeSamplerCreate(
    ze_device_handle_t hDevice,
    const ze_sampler_desc_t *desc,
    ze_sampler_handle_t *phSampler) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phSampler)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_SAMPLER_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Device::fromHandle(hDevice)->createSampler(desc, phSampler);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeSamplerDestroy(
    ze_sampler_handle_t hSampler) {
    try {
        {
            if (nullptr == hSampler)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sampler::fromHandle(hSampler)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}