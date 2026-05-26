/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/api/additional_extensions.h"
#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/source/sharings/sharing_factory.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_int CL_API_CALL clGetPlatformIDs(cl_uint numEntries,
                                    cl_platform_id *platforms,
                                    cl_uint *numPlatforms) {
    if ((numEntries == 0 && platforms != nullptr) ||
        (numEntries > 0 && platforms == nullptr && numPlatforms == nullptr)) [[unlikely]] {
        return CL_INVALID_VALUE;
    }

    if (NEO::debugManager.flags.EnableLEO.get() != 1) {
        if (numPlatforms) {
            *numPlatforms = 0;
        }
        return CL_SUCCESS;
    }

    if (NEO::LEO::platformsImpl->empty()) {
        uint32_t driverCount = 0;
        ze_init_driver_type_desc_t desc{ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC, nullptr, ZE_INIT_DRIVER_TYPE_FLAG_GPU};

        ze_result_t ret = zeInitDrivers(&driverCount, nullptr, &desc);

        if (ret != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(ret);
        }

        std::vector<ze_driver_handle_t> driverHandles(driverCount);
        zeInitDrivers(&driverCount, driverHandles.data(), &desc);

        for (int i = 0; i < std::ssize(driverHandles); ++i) {
            NEO::LEO::platformsImpl->push_back(std::make_unique<NEO::LEO::Platform>(driverHandles[i]));
        }
    }

    if (numPlatforms) {
        *numPlatforms = static_cast<cl_uint>(NEO::LEO::platformsImpl->size());
    }
    if (platforms) {
        auto numPlatformsToReturn = std::min(static_cast<cl_uint>(NEO::LEO::platformsImpl->size()), numEntries);
        for (cl_uint i = 0; i < numPlatformsToReturn; ++i) {
            platforms[i] = (*NEO::LEO::platformsImpl)[i].get();
        }
    }

    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clIcdGetPlatformIDsKHR(cl_uint numEntries,
                                                       cl_platform_id *platforms,
                                                       cl_uint *numPlatforms) {
    return clGetPlatformIDs(numEntries, platforms, numPlatforms);
}

cl_int CL_API_CALL clGetPlatformInfo(cl_platform_id platform,
                                     cl_platform_info paramName,
                                     size_t paramValueSize,
                                     void *paramValue,
                                     size_t *paramValueSizeRet) {
    auto [retVal, pPlatform] = NEO::LEO::validateAndCast(std::make_tuple(platform));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pPlatform->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clUnloadPlatformCompiler(cl_platform_id platform) {
    auto [retVal, pPlatform] = NEO::LEO::validateAndCast(std::make_tuple(platform));
    return retVal;
}

cl_int CL_API_CALL clUnloadCompiler() {
    return CL_SUCCESS;
}

#define RETURN_FUNC_PTR_IF_EXIST(name)                  \
    {                                                   \
        if (!strcmp(funcName, #name)) {                 \
            void *ret = reinterpret_cast<void *>(name); \
            return ret;                                 \
        }                                               \
    }
void *CL_API_CALL clGetExtensionFunctionAddress(const char *funcName) {
    // Support an internal call by the ICD
    RETURN_FUNC_PTR_IF_EXIST(clIcdGetPlatformIDsKHR);

    // perf counters
    RETURN_FUNC_PTR_IF_EXIST(clCreatePerfCountersCommandQueueINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSetPerformanceConfigurationINTEL);
    // Support device extensions
    RETURN_FUNC_PTR_IF_EXIST(clCreateAcceleratorINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetAcceleratorInfoINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clRetainAcceleratorINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clReleaseAcceleratorINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clCreateBufferWithPropertiesINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clCreateImageWithPropertiesINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clAddCommentINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueVerifyMemoryINTEL);

    RETURN_FUNC_PTR_IF_EXIST(clHostMemAllocINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clDeviceMemAllocINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSharedMemAllocINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clMemFreeINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clMemBlockingFreeINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetMemAllocInfoINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSetKernelArgMemPointerINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemsetINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemFillINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemcpyINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMigrateMemINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemAdviseINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetDeviceFunctionPointerINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetDeviceGlobalVariablePointerINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetKernelMaxConcurrentWorkGroupCountINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetKernelSuggestedLocalWorkSizeINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueNDCountKernelINTEL);

    RETURN_FUNC_PTR_IF_EXIST(clEnqueueAcquireExternalMemObjectsKHR);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueReleaseExternalMemObjectsKHR);

    void *ret = NEO::LEO::sharingFactory.getExtensionFunctionAddress(funcName);
    if (ret != nullptr) {
        return ret;
    }

    // SPIR-V support through the cl_khr_il_program extension
    RETURN_FUNC_PTR_IF_EXIST(clCreateProgramWithILKHR);
    RETURN_FUNC_PTR_IF_EXIST(clCreateCommandQueueWithPropertiesKHR);

    RETURN_FUNC_PTR_IF_EXIST(clSetProgramSpecializationConstant);

    RETURN_FUNC_PTR_IF_EXIST(clGetKernelSuggestedLocalWorkSize);
    RETURN_FUNC_PTR_IF_EXIST(clGetKernelSuggestedLocalWorkSizeKHR);
    ret = NEO::getAdditionalExtensionFunctionAddress(funcName);
    return ret;
}

// OpenCL 1.2
void *CL_API_CALL clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                                           const char *funcName) {
    return clGetExtensionFunctionAddress(funcName);
}
