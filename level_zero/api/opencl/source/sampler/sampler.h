/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/core/source/sampler/sampler.h"

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_sampler> {
    typedef class Sampler DerivedType;
};

class Sampler : public BaseObject<_cl_sampler> {
  public:
    static const cl_ulong objectMagic = 0x4684913AC213EF00LL;

    Sampler(Context *context, ze_sampler_handle_t sampler, const cl_sampler_properties *properties);
    Sampler() = delete;
    ~Sampler() override;

    cl_int getInfo(cl_sampler_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    void resetProperties();

    ze_sampler_handle_t getL0Handle() const { return this->samplerHandle; };
    L0::Sampler *getL0Object() const { return L0::Sampler::fromHandle(this->samplerHandle); };

  protected:
    void storeProperties(const cl_sampler_properties *properties);

    std::vector<cl_sampler_properties> samplerProperties{};
    Context *context = nullptr;

    ze_sampler_handle_t samplerHandle = nullptr;
};

} // namespace LEO
} // namespace NEO
