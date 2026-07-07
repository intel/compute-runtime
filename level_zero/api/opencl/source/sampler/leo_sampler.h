/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/leo_cl_types.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/core/source/sampler/sampler.h"

#include <map>

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_sampler> {
    typedef class Sampler DerivedType;
};

class Sampler : public BaseObject<_cl_sampler> {
  public:
    static const cl_ulong objectMagic = 0x4684913AC213EF00LL;

    Sampler(Context *context, std::map<uint32_t, ze_sampler_handle_t> samplerHandles, const cl_sampler_properties *properties);
    Sampler() = delete;
    ~Sampler() override;

    cl_int getInfo(cl_sampler_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    void resetProperties();

    ze_sampler_handle_t getL0Handle() const { return this->samplerHandles.begin()->second; };
    ze_sampler_handle_t getL0Handle(uint32_t rootDeviceIndex) const {
        auto it = this->samplerHandles.find(rootDeviceIndex);
        return it == this->samplerHandles.end() ? this->samplerHandles.begin()->second : it->second;
    };
    L0::Sampler *getL0Object() const { return L0::Sampler::fromHandle(this->getL0Handle()); };

  protected:
    void storeProperties(const cl_sampler_properties *properties);

    std::vector<cl_sampler_properties> samplerProperties{};
    Context *context = nullptr;

    std::map<uint32_t, ze_sampler_handle_t> samplerHandles{};
};

} // namespace LEO
} // namespace NEO
