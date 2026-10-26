/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/sampler/sampler_imp.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

SamplerAllocatorFn samplerFactory[NEO::maxCoreEnumValue] = {};

ze_result_t SamplerImp::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t SamplerImp::initialize(Device *device, const ze_sampler_desc_t *desc) {
    samplerDesc = *desc;
    return ZE_RESULT_SUCCESS;
}

void SamplerImp::adjustSamplerState(void *samplerState, Device *device) {
    auto &hwInfo = device->getHwInfo();
    auto &helper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
    helper.adjustSamplerState(samplerState, hwInfo);
}

Sampler *Sampler::create(Device *device, const ze_sampler_desc_t *desc) {
    SamplerAllocatorFn allocator = nullptr;
    auto gfxCoreFamily = device->getNEODevice()->getRenderCoreFamily();
    if (gfxCoreFamily < NEO::maxCoreEnumValue) {
        allocator = samplerFactory[gfxCoreFamily];
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
