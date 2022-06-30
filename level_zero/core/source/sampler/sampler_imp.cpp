/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/sampler/sampler_imp.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

SamplerAllocatorFn samplerFactory[IGFX_MAX_PRODUCT] = {};

ze_result_t SamplerImp::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t SamplerImp::initialize(Device *device, const ze_sampler_desc_t *desc) {
    samplerDesc = *desc;
    return ZE_RESULT_SUCCESS;
}

Sampler *Sampler::create(uint32_t productFamily, Device *device, const ze_sampler_desc_t *desc) {
    SamplerAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = samplerFactory[productFamily];
    }

    SamplerImp *sampler = nullptr;
    if (allocator) {
        sampler = static_cast<SamplerImp *>((*allocator)());
        if (sampler->initialize(device, desc) != ZE_RESULT_SUCCESS) {
            sampler->destroy();
            sampler = nullptr;
        }
    }

    return sampler;
}

} // namespace L0
