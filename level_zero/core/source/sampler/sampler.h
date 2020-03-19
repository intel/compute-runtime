/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_sampler.h>

struct _ze_sampler_handle_t {};

namespace L0 {

struct Sampler : _ze_sampler_handle_t {
    template <typename Type>
    struct Allocator {
        static Sampler *allocate() { return new Type(); }
    };

    virtual ~Sampler() = default;
    virtual ze_result_t destroy() = 0;

    static Sampler *create(uint32_t productFamily, Device *device,
                           const ze_sampler_desc_t *desc);

    virtual void copySamplerStateToDSH(void *dynamicStateHeap,
                                       const uint32_t dynamicStateHeapSize,
                                       const uint32_t heapOffset) = 0;

    static Sampler *fromHandle(ze_sampler_handle_t handle) {
        return static_cast<Sampler *>(handle);
    }

    inline ze_sampler_handle_t toHandle() { return this; }
};

using SamplerAllocatorFn = Sampler *(*)();
extern SamplerAllocatorFn samplerFactory[];

template <uint32_t productFamily, typename SamplerType>
struct SamplerPopulateFactory {
    SamplerPopulateFactory() {
        samplerFactory[productFamily] =
            Sampler::Allocator<SamplerType>::allocate;
    }
};

} // namespace L0
