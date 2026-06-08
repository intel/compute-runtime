/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/sampler/sampler.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <limits>

cl_sampler CL_API_CALL clCreateSampler(cl_context context,
                                       cl_bool normalizedCoords,
                                       cl_addressing_mode addressingMode,
                                       cl_filter_mode filterMode,
                                       cl_int *errcodeRet) {
    cl_sampler_properties samplerProperties[] = {CL_SAMPLER_NORMALIZED_COORDS, normalizedCoords,
                                                 CL_SAMPLER_ADDRESSING_MODE, addressingMode,
                                                 CL_SAMPLER_FILTER_MODE, filterMode,
                                                 0};
    auto sampler = clCreateSamplerWithProperties(context, samplerProperties, errcodeRet);

    if (sampler) {
        NEO::LEO::castToObject<NEO::LEO::Sampler>(sampler)->resetProperties();
    }

    return sampler;
}

cl_sampler CL_API_CALL clCreateSamplerWithProperties(cl_context context,
                                                     const cl_sampler_properties *samplerProperties,
                                                     cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    ze_sampler_desc_t samplerDesc{ZE_STRUCTURE_TYPE_SAMPLER_DESC};

    L0::ze_sampler_lod_ext_desc_t lodExtDesc{};
    lodExtDesc.lodMax = std::numeric_limits<float>::max();

    if (samplerProperties != nullptr) {
        auto samplerPropertiesAddress = samplerProperties;

        while (*samplerPropertiesAddress != 0) {
            auto propertyType = samplerPropertiesAddress[0];
            auto propertyValue = samplerPropertiesAddress[1];
            samplerPropertiesAddress += 2;

            switch (propertyType) {
            case CL_SAMPLER_NORMALIZED_COORDS:
                samplerDesc.isNormalized = static_cast<bool>(propertyValue);
                break;
            case CL_SAMPLER_ADDRESSING_MODE:
                switch (propertyValue) {
                case CL_ADDRESS_NONE:
                    samplerDesc.addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE;
                    break;
                case CL_ADDRESS_CLAMP_TO_EDGE:
                    samplerDesc.addressMode = ZE_SAMPLER_ADDRESS_MODE_CLAMP;
                    break;
                case CL_ADDRESS_CLAMP:
                    samplerDesc.addressMode = ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                    break;
                case CL_ADDRESS_REPEAT:
                    samplerDesc.addressMode = ZE_SAMPLER_ADDRESS_MODE_REPEAT;
                    break;
                case CL_ADDRESS_MIRRORED_REPEAT:
                    samplerDesc.addressMode = ZE_SAMPLER_ADDRESS_MODE_MIRROR;
                    break;
                default:
                    err.set(CL_INVALID_PROPERTY);
                    return nullptr;
                }
                break;

            case CL_SAMPLER_FILTER_MODE:
                switch (propertyValue) {
                case CL_FILTER_NEAREST:
                    samplerDesc.filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST;
                    break;
                case CL_FILTER_LINEAR:
                    samplerDesc.filterMode = ZE_SAMPLER_FILTER_MODE_LINEAR;
                    break;
                default:
                    err.set(CL_INVALID_PROPERTY);
                    return nullptr;
                }
                break;

            case CL_SAMPLER_MIP_FILTER_MODE_KHR:
                switch (propertyValue) {
                case CL_FILTER_NEAREST:
                    lodExtDesc.mipFilterLinear = false;
                    break;
                case CL_FILTER_LINEAR:
                    lodExtDesc.mipFilterLinear = true;
                    break;
                default:
                    err.set(CL_INVALID_PROPERTY);
                    return nullptr;
                }
                break;
            case CL_SAMPLER_LOD_MIN_KHR: {
                lodExtDesc.lodMin = static_cast<float>(propertyValue);
                break;
            }
            case CL_SAMPLER_LOD_MAX_KHR: {
                lodExtDesc.lodMax = static_cast<float>(propertyValue);
                break;
            }
            default:
                err.set(CL_INVALID_PROPERTY);
                return nullptr;
            }
        }
    }

    lodExtDesc.pNext = samplerDesc.pNext;
    samplerDesc.pNext = &lodExtDesc;

    std::map<uint32_t, ze_sampler_handle_t> samplerHandles{};
    for (auto clDevice : pContext->getClDevices()) {
        const auto rootDeviceIndex = clDevice->getRootDeviceIndex();
        if (samplerHandles.count(rootDeviceIndex) != 0) {
            continue;
        }
        ze_sampler_handle_t handle{};
        auto createResult = zeSamplerCreate(pContext->getL0ContextHandle(), clDevice->getL0Handle(), &samplerDesc, &handle);
        if (createResult != ZE_RESULT_SUCCESS) {
            for (auto &[idx, h] : samplerHandles) {
                zeSamplerDestroy(h);
            }
            err.set(L0ToClResultMapper(createResult));
            return nullptr;
        }
        samplerHandles[rootDeviceIndex] = handle;
    }

    return new NEO::LEO::Sampler(pContext, std::move(samplerHandles), samplerProperties);
}

cl_int CL_API_CALL clRetainSampler(cl_sampler sampler) {
    auto [retVal, pSampler] = NEO::LEO::validateAndCast(std::make_tuple(sampler));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pSampler->incRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseSampler(cl_sampler sampler) {
    auto [retVal, pSampler] = NEO::LEO::validateAndCast(std::make_tuple(sampler));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pSampler->decRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetSamplerInfo(cl_sampler sampler,
                                    cl_sampler_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    auto [retVal, pSampler] = NEO::LEO::validateAndCast(std::make_tuple(sampler));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pSampler->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}
