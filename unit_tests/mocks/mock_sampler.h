/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/sampler/sampler.h"

namespace OCLRT {

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
} // namespace OCLRT
