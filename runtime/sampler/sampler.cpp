/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sampler/sampler.h"

#include "core/helpers/get_info.h"
#include "core/helpers/hw_info.h"
#include "runtime/context/context.h"
#include "runtime/device/cl_device.h"

#include "patch_list.h"

#include <limits>

namespace NEO {

SamplerCreateFunc samplerFactory[IGFX_MAX_CORE] = {};
getSamplerStateSizeHwFunc getSamplerStateSizeHw[IGFX_MAX_CORE] = {};

Sampler::Sampler(Context *context, cl_bool normalizedCoordinates,
                 cl_addressing_mode addressingMode, cl_filter_mode filterMode,
                 cl_filter_mode mipFilterMode, float lodMin, float lodMax)
    : context(context), normalizedCoordinates(normalizedCoordinates),
      addressingMode(addressingMode), filterMode(filterMode),
      mipFilterMode(mipFilterMode), lodMin(lodMin), lodMax(lodMax) {
}

Sampler::Sampler(Context *context,
                 cl_bool normalizedCoordinates,
                 cl_addressing_mode addressingMode,
                 cl_filter_mode filterMode)
    : Sampler(context, normalizedCoordinates, addressingMode, filterMode,
              CL_FILTER_NEAREST, 0.0f, std::numeric_limits<float>::max()) {
}

Sampler *Sampler::create(Context *context, cl_bool normalizedCoordinates,
                         cl_addressing_mode addressingMode, cl_filter_mode filterMode,
                         cl_filter_mode mipFilterMode, float lodMin, float lodMax,
                         cl_int &errcodeRet) {
    errcodeRet = CL_SUCCESS;
    Sampler *sampler = nullptr;

    DEBUG_BREAK_IF(nullptr == context);
    const auto device = context->getDevice(0);
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = samplerFactory[hwInfo.platform.eRenderCoreFamily];
    DEBUG_BREAK_IF(nullptr == funcCreate);
    sampler = funcCreate(context, normalizedCoordinates, addressingMode, filterMode, mipFilterMode, lodMin, lodMax);

    if (sampler == nullptr) {
        errcodeRet = CL_OUT_OF_HOST_MEMORY;
    }

    return sampler;
}

size_t Sampler::getSamplerStateSize(const HardwareInfo &hwInfo) {
    return getSamplerStateSizeHw[hwInfo.platform.eRenderCoreFamily]();
}

template <typename ParameterType>
struct SetOnce {
    SetOnce(ParameterType defaultValue, ParameterType min, ParameterType max)
        : value(defaultValue), min(min), max(max) {
    }

    cl_int setValue(ParameterType property) {
        if (alreadySet) {
            return CL_INVALID_VALUE;
        }

        if ((property < min) || (property > max)) {
            return CL_INVALID_VALUE;
        }

        this->value = property;
        alreadySet = true;

        return CL_SUCCESS;
    }

    bool alreadySet = false;
    ParameterType value;
    ParameterType min;
    ParameterType max;
};

Sampler *Sampler::create(Context *context,
                         const cl_sampler_properties *samplerProperties,
                         cl_int &errcodeRet) {
    SetOnce<uint32_t> normalizedCoords(CL_TRUE, CL_FALSE, CL_TRUE);
    SetOnce<uint32_t> filterMode(CL_FILTER_NEAREST, CL_FILTER_NEAREST, CL_FILTER_LINEAR);
    SetOnce<uint32_t> addressingMode(CL_ADDRESS_CLAMP, CL_ADDRESS_NONE, CL_ADDRESS_MIRRORED_REPEAT);
    SetOnce<uint32_t> mipFilterMode(CL_FILTER_NEAREST, CL_FILTER_NEAREST, CL_FILTER_LINEAR);
    SetOnce<float> lodMin(0.0f, 0.0f, std::numeric_limits<float>::max());
    SetOnce<float> lodMax(std::numeric_limits<float>::max(), 0.0f, std::numeric_limits<float>::max());

    errcodeRet = CL_SUCCESS;
    if (samplerProperties) {
        cl_ulong samType;

        while ((samType = *samplerProperties) != 0) {
            ++samplerProperties;
            auto samValue = *samplerProperties;
            switch (samType) {
            case CL_SAMPLER_NORMALIZED_COORDS:
                errcodeRet = normalizedCoords.setValue(static_cast<uint32_t>(samValue));
                break;
            case CL_SAMPLER_ADDRESSING_MODE:
                errcodeRet = addressingMode.setValue(static_cast<uint32_t>(samValue));
                break;
            case CL_SAMPLER_FILTER_MODE:
                errcodeRet = filterMode.setValue(static_cast<uint32_t>(samValue));
                break;
            case CL_SAMPLER_MIP_FILTER_MODE:
                errcodeRet = mipFilterMode.setValue(static_cast<uint32_t>(samValue));
                break;
            case CL_SAMPLER_LOD_MIN: {
                SamplerLodProperty lodData;
                lodData.data = samValue;
                errcodeRet = lodMin.setValue(lodData.lod);
                break;
            }
            case CL_SAMPLER_LOD_MAX: {
                SamplerLodProperty lodData;
                lodData.data = samValue;
                errcodeRet = lodMax.setValue(lodData.lod);
                break;
            }
            default:
                errcodeRet = CL_INVALID_VALUE;
                break;
            }
            ++samplerProperties;
        }
    }

    Sampler *sampler = nullptr;
    if (errcodeRet == CL_SUCCESS) {
        sampler = create(context, normalizedCoords.value, addressingMode.value, filterMode.value,
                         mipFilterMode.value, lodMin.value, lodMax.value,
                         errcodeRet);
    }

    return sampler;
}

unsigned int Sampler::getSnapWaValue() const {
    if (filterMode == CL_FILTER_NEAREST && addressingMode == CL_ADDRESS_CLAMP) {
        return iOpenCL::CONSTANT_REGISTER_BOOLEAN_TRUE;
    } else {
        return iOpenCL::CONSTANT_REGISTER_BOOLEAN_FALSE;
    }
}

cl_int Sampler::getInfo(cl_sampler_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t valueSize = 0;
    const void *pValue = nullptr;
    cl_uint refCount = 0;

    switch (paramName) {
    case CL_SAMPLER_CONTEXT:
        valueSize = sizeof(cl_device_id);
        pValue = &this->context;
        break;

    case CL_SAMPLER_NORMALIZED_COORDS:
        valueSize = sizeof(cl_bool);
        pValue = &this->normalizedCoordinates;
        break;

    case CL_SAMPLER_ADDRESSING_MODE:
        valueSize = sizeof(cl_addressing_mode);
        pValue = &this->addressingMode;
        break;

    case CL_SAMPLER_FILTER_MODE:
        valueSize = sizeof(cl_filter_mode);
        pValue = &this->filterMode;
        break;

    case CL_SAMPLER_MIP_FILTER_MODE:
        valueSize = sizeof(cl_filter_mode);
        pValue = &this->mipFilterMode;
        break;

    case CL_SAMPLER_LOD_MIN:
        valueSize = sizeof(float);
        pValue = &this->lodMin;
        break;

    case CL_SAMPLER_LOD_MAX:
        valueSize = sizeof(float);
        pValue = &this->lodMax;
        break;

    case CL_SAMPLER_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        valueSize = sizeof(refCount);
        pValue = &refCount;
        break;

    default:
        break;
    }

    retVal = ::getInfo(paramValue, paramValueSize, pValue, valueSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = valueSize;
    }

    return retVal;
}

bool Sampler::isTransformable() const {
    return addressingMode == CL_ADDRESS_CLAMP_TO_EDGE &&
           filterMode == CL_FILTER_NEAREST &&
           normalizedCoordinates == CL_FALSE;
}

} // namespace NEO
