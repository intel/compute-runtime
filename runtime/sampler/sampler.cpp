/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/sampler/sampler.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/hw_info.h"
#include "patch_list.h"

namespace OCLRT {

SamplerCreateFunc samplerFactory[IGFX_MAX_CORE] = {};
getSamplerStateSizeHwFunc getSamplerStateSizeHw[IGFX_MAX_CORE] = {};

Sampler::Sampler(Context *context, cl_bool normalizedCoordinates,
                 cl_addressing_mode addressingMode, cl_filter_mode filterMode)
    : context(context), normalizedCoordinates(normalizedCoordinates),
      addressingMode(addressingMode), filterMode(filterMode) {
}

Sampler *Sampler::create(Context *context, cl_bool normalizedCoordinates,
                         cl_addressing_mode addressingMode,
                         cl_filter_mode filterMode, cl_int &errcodeRet) {
    errcodeRet = CL_SUCCESS;
    Sampler *sampler = nullptr;

    DEBUG_BREAK_IF(nullptr == context);
    const auto device = context->getDevice(0);
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = samplerFactory[hwInfo.pPlatform->eRenderCoreFamily];
    DEBUG_BREAK_IF(nullptr == funcCreate);
    sampler = funcCreate(context, normalizedCoordinates, addressingMode, filterMode);

    if (sampler == nullptr) {
        errcodeRet = CL_OUT_OF_HOST_MEMORY;
    }

    return sampler;
}

size_t Sampler::getSamplerStateSize(const HardwareInfo &hwInfo) {
    return getSamplerStateSizeHw[hwInfo.pPlatform->eRenderCoreFamily]();
}

template <typename ParameterType, ParameterType minValue, ParameterType maxValue>
struct SetOnce {
    SetOnce(ParameterType defaultValue) : value(defaultValue),
                                          setValueInternal(&SetOnce::setValueValid) {
    }

    cl_int setValue(cl_ulong property) {
        auto result = (this->*setValueInternal)(property);
        setValueInternal = &SetOnce::setValueInvalid;
        return result;
    }

    ParameterType value;

  protected:
    cl_int (SetOnce::*setValueInternal)(cl_ulong property);

    cl_int setValueValid(cl_ulong property) {
        if (property >= minValue && property <= maxValue) {
            value = static_cast<ParameterType>(property);
            return CL_SUCCESS;
        }
        return CL_INVALID_VALUE;
    }

    cl_int setValueInvalid(cl_ulong property) {
        return CL_INVALID_VALUE;
    }
};

Sampler *Sampler::create(Context *context,
                         const cl_sampler_properties *samplerProperties,
                         cl_int &errcodeRet) {
    SetOnce<uint32_t, CL_FALSE, CL_TRUE> normalizedCoords(CL_TRUE);
    SetOnce<uint32_t, CL_FILTER_NEAREST, CL_FILTER_LINEAR> filterMode(CL_FILTER_NEAREST);
    SetOnce<uint32_t, CL_ADDRESS_NONE, CL_ADDRESS_MIRRORED_REPEAT> addressingMode(CL_ADDRESS_CLAMP);

    errcodeRet = CL_SUCCESS;
    if (samplerProperties) {
        cl_ulong samType;

        while ((samType = *samplerProperties) != 0) {
            ++samplerProperties;
            auto samValue = *samplerProperties;
            switch (samType) {
            case CL_SAMPLER_NORMALIZED_COORDS:
                errcodeRet = normalizedCoords.setValue(samValue);
                break;
            case CL_SAMPLER_ADDRESSING_MODE:
                errcodeRet = addressingMode.setValue(samValue);
                break;
            case CL_SAMPLER_FILTER_MODE:
                errcodeRet = filterMode.setValue(samValue);
                break;
            default:
                errcodeRet = CL_INVALID_VALUE;
                break;
            }
            ++samplerProperties;
        }
    }

    Sampler *sampler = nullptr;
    if (errcodeRet == CL_SUCCESS) {
        sampler = create(context, normalizedCoords.value, addressingMode.value, filterMode.value, errcodeRet);
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
} // namespace OCLRT
