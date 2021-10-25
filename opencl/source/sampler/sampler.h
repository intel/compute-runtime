/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/api/cl_types.h"
#include "opencl/source/helpers/base_object.h"

#include <vector>

namespace NEO {
class Context;
struct HardwareInfo;

template <>
struct OpenCLObjectMapper<_cl_sampler> {
    typedef class Sampler DerivedType;
};

union SamplerLodProperty {
    cl_sampler_properties data;
    float lod;
};

class Sampler : public BaseObject<_cl_sampler> {
  public:
    static const cl_ulong objectMagic = 0x4684913AC213EF00LL;
    static const uint32_t samplerStateArrayAlignment = 64;

    static Sampler *create(Context *context, cl_bool normalizedCoordinates,
                           cl_addressing_mode addressingMode, cl_filter_mode filterMode,
                           cl_filter_mode mipFilterMode, float lodMin, float lodMax,
                           cl_int &errcodeRet);

    static Sampler *create(Context *context, cl_bool normalizedCoordinates,
                           cl_addressing_mode addressingMode, cl_filter_mode filterMode,
                           cl_int &errcodeRet) {
        return Sampler::create(context, normalizedCoordinates, addressingMode, filterMode,
                               CL_FILTER_NEAREST, 0.0f, std::numeric_limits<float>::max(),
                               errcodeRet);
    }

    static Sampler *create(Context *context,
                           const cl_sampler_properties *samplerProperties,
                           cl_int &errcodeRet);

    cl_int getInfo(cl_sampler_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    virtual void setArg(void *memory, const HardwareInfo &hwInfo) = 0;

    bool isTransformable() const;

    Sampler(Context *context,
            cl_bool normalizedCoordinates,
            cl_addressing_mode addressingMode,
            cl_filter_mode filterMode,
            cl_filter_mode mipFilterMode,
            float lodMin,
            float lodMax);

    Sampler(Context *context,
            cl_bool normalizedCoordinates,
            cl_addressing_mode addressingMode,
            cl_filter_mode filterMode);

    unsigned int getSnapWaValue() const;

    cl_context context;
    cl_bool normalizedCoordinates;
    cl_addressing_mode addressingMode;
    cl_filter_mode filterMode;
    cl_filter_mode mipFilterMode;
    float lodMin;
    float lodMax;

  protected:
    void storeProperties(const cl_sampler_properties *properties);

    std::vector<uint64_t> propertiesVector;
};

template <typename GfxFamily>
struct SamplerHw : public Sampler {
    void setArg(void *memory, const HardwareInfo &hwInfo) override;
    static constexpr float getGenSamplerMaxLod() {
        return 14.0f;
    }

    SamplerHw(Context *context,
              cl_bool normalizedCoordinates,
              cl_addressing_mode addressingMode,
              cl_filter_mode filterMode,
              cl_filter_mode mipFilterMode,
              float lodMin,
              float lodMax)
        : Sampler(context, normalizedCoordinates, addressingMode, filterMode,
                  mipFilterMode, lodMin, lodMax) {
    }

    SamplerHw(Context *context,
              cl_bool normalizedCoordinates,
              cl_addressing_mode addressingMode,
              cl_filter_mode filterMode)
        : Sampler(context, normalizedCoordinates, addressingMode, filterMode) {
    }

    static Sampler *create(Context *context,
                           cl_bool normalizedCoordinates,
                           cl_addressing_mode addressingMode,
                           cl_filter_mode filterMode,
                           cl_filter_mode mipFilterMode,
                           float lodMin,
                           float lodMax) {
        return new SamplerHw<GfxFamily>(context,
                                        normalizedCoordinates,
                                        addressingMode,
                                        filterMode,
                                        mipFilterMode,
                                        lodMin,
                                        lodMax);
    }
};

typedef Sampler *(*SamplerCreateFunc)(Context *context,
                                      cl_bool normalizedCoordinates,
                                      cl_addressing_mode addressingMode,
                                      cl_filter_mode filterMode,
                                      cl_filter_mode mipFilterMode,
                                      float lodMin,
                                      float lodMax);

typedef size_t (*getSamplerStateSizeHwFunc)();

template <>
inline Sampler *castToObject<Sampler>(const void *object) {
    auto clSamplerObj = reinterpret_cast<const _cl_sampler *>(object);
    return castToObject<Sampler>(const_cast<cl_sampler>(clSamplerObj));
}
} // namespace NEO
