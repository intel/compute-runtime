/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/sampler/sampler.h"

namespace NEO {

struct MockSampler : public Sampler {
  public:
    MockSampler(Context *context,
                cl_bool normalizedCoordinates,
                cl_addressing_mode addressingMode,
                cl_filter_mode filterMode,
                cl_filter_mode mipFilterMode = CL_FILTER_NEAREST,
                float lodMin = 0.0f,
                float lodMax = 0.0f) : Sampler(context, normalizedCoordinates, addressingMode, filterMode,
                                               mipFilterMode, lodMin, lodMax) {
    }

    cl_context getContext() const {
        return context;
    }

    cl_bool getNormalizedCoordinates() const {
        return normalizedCoordinates;
    }

    cl_addressing_mode getAddressingMode() const {
        return addressingMode;
    }

    cl_filter_mode getFilterMode() const {
        return filterMode;
    }

    void setArg(void *memory) override {
    }
};
} // namespace NEO
