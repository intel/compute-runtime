/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

struct _ze_sampler_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_SAMPLER> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_sampler_handle_t>);

namespace L0 {
struct Device;

struct Sampler : _ze_sampler_handle_t {
    template <typename Type>
    struct Allocator {
        static Sampler *allocate() { return new Type(); }
    };

    virtual ~Sampler() = default;
    virtual ze_result_t destroy() = 0;

    static Sampler *create(uint32_t productFamily, Device *device,
                           const ze_sampler_desc_t *desc);

    virtual void copySamplerStateToDSH(ArrayRef<uint8_t> dynamicStateHeap,
                                       const uint32_t heapOffset) = 0;

    static Sampler *fromHandle(ze_sampler_handle_t handle) {
        return static_cast<Sampler *>(handle);
    }

    inline ze_sampler_handle_t toHandle() { return this; }
    const ze_sampler_desc_t getSamplerDesc() const {
        return samplerDesc;
    }

  protected:
    ze_sampler_desc_t samplerDesc = {};
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
