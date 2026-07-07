/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sampler/leo_sampler.h"

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/helpers/leo_get_info_status_mapper.h"
#include "level_zero/core/source/sampler/sampler.h"

namespace NEO {
namespace LEO {

Sampler::Sampler(Context *context, std::map<uint32_t, ze_sampler_handle_t> samplerHandles, const cl_sampler_properties *properties) : context(context), samplerHandles(std::move(samplerHandles)) {
    context->incRefInternal();
    this->storeProperties(properties);
}

Sampler::~Sampler() {
    context->decRefInternal();
    for (auto &[rootDeviceIndex, samplerHandle] : this->samplerHandles) {
        zeSamplerDestroy(samplerHandle);
    }
}

cl_int Sampler::getInfo(cl_sampler_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    size_t valueSize = GetInfo::invalidSourceSize;
    const void *pValue = nullptr;

    cl_context clContext = this->context;
    cl_uint refCount = 0;

    auto samplerDesc = L0::Sampler::fromHandle(this->getL0Handle())->getSamplerDesc();

    cl_bool normalized = samplerDesc.isNormalized;

    cl_addressing_mode addressingMode{};
    switch (samplerDesc.addressMode) {
    case ZE_SAMPLER_ADDRESS_MODE_NONE:
        addressingMode = CL_ADDRESS_NONE;
        break;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP:
        addressingMode = CL_ADDRESS_CLAMP_TO_EDGE;
        break;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
        addressingMode = CL_ADDRESS_CLAMP;
        break;
    case ZE_SAMPLER_ADDRESS_MODE_REPEAT:
        addressingMode = CL_ADDRESS_REPEAT;
        break;
    case ZE_SAMPLER_ADDRESS_MODE_MIRROR:
        addressingMode = CL_ADDRESS_MIRRORED_REPEAT;
        break;
    default:
        break;
    }

    cl_filter_mode filterMode{};
    switch (samplerDesc.filterMode) {
    case ZE_SAMPLER_FILTER_MODE_NEAREST:
        filterMode = CL_FILTER_NEAREST;
        break;
    case ZE_SAMPLER_FILTER_MODE_LINEAR:
        filterMode = CL_FILTER_LINEAR;
        break;
    default:
        break;
    }

    switch (paramName) {
    case CL_SAMPLER_CONTEXT:
        valueSize = sizeof(cl_context);
        pValue = &clContext;
        break;

    case CL_SAMPLER_NORMALIZED_COORDS:
        valueSize = sizeof(normalized);
        pValue = &normalized;
        break;

    case CL_SAMPLER_ADDRESSING_MODE:
        valueSize = sizeof(cl_addressing_mode);
        pValue = &addressingMode;
        break;

    case CL_SAMPLER_FILTER_MODE:
        valueSize = sizeof(cl_filter_mode);
        pValue = &filterMode;
        break;

    case CL_SAMPLER_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        valueSize = sizeof(refCount);
        pValue = &refCount;
        break;
    case CL_SAMPLER_PROPERTIES:
        valueSize = this->samplerProperties.size() * sizeof(cl_sampler_properties);
        pValue = this->samplerProperties.data();
        break;

    case CL_SAMPLER_MIP_FILTER_MODE:
    case CL_SAMPLER_LOD_MAX:
    default:
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pValue, valueSize);
    auto retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, valueSize, getInfoStatus);

    return retVal;
}

void Sampler::resetProperties() {
    this->samplerProperties.clear();
}

void Sampler::storeProperties(const cl_sampler_properties *properties) {
    if (properties != nullptr) {
        while (*properties != 0) {
            this->samplerProperties.push_back(*properties);
            ++properties;
            this->samplerProperties.push_back(*properties);
            ++properties;
        }
        this->samplerProperties.push_back(0u);
    }
}

} // namespace LEO
} // namespace NEO
