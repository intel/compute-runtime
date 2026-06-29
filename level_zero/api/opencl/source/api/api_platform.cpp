/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/api/opencl/source/api/additional_extensions.h"
#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/source/sharings/sharing_factory.h"
#include "level_zero/api/opencl/source/tracing/tracing_api.h"
#include "level_zero/api/opencl/source/tracing/tracing_notify.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_int CL_API_CALL clGetPlatformIDs(cl_uint numEntries,
                                    cl_platform_id *platforms,
                                    cl_uint *numPlatforms) {
    TRACING_ENTER(ClGetPlatformIDs, &numEntries, &platforms, &numPlatforms);
    if ((numEntries == 0 && platforms != nullptr) ||
        (numEntries > 0 && platforms == nullptr && numPlatforms == nullptr)) [[unlikely]] {
        cl_int retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClGetPlatformIDs, &retVal);
        return retVal;
    }

    auto enableLEOFlag = NEO::debugManager.flags.EnableLEO.get();
    if (enableLEOFlag == 0) {
        if (numPlatforms) {
            *numPlatforms = 0;
        }
        cl_int retVal = CL_SUCCESS;
        TRACING_EXIT(ClGetPlatformIDs, &retVal);
        return retVal;
    }

    if (!NEO::LEO::platformsImpl) {
        if (numPlatforms) {
            *numPlatforms = 0;
        }
        cl_int retVal = CL_SUCCESS;
        TRACING_EXIT(ClGetPlatformIDs, &retVal);
        return retVal;
    }

    if (NEO::LEO::platformsImpl->empty()) {
        uint32_t driverCount = 0;
        ze_init_driver_type_desc_t desc{ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC, nullptr, ZE_INIT_DRIVER_TYPE_FLAG_GPU};

        ze_result_t ret = zeInitDrivers(&driverCount, nullptr, &desc);

        if (ret != ZE_RESULT_SUCCESS) {
            cl_int retVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClGetPlatformIDs, &retVal);
            return retVal;
        }

        std::vector<ze_driver_handle_t> driverHandles(driverCount);
        zeInitDrivers(&driverCount, driverHandles.data(), &desc);

        for (int i = 0; i < std::ssize(driverHandles); ++i) {
            NEO::LEO::platformsImpl->push_back(std::make_unique<NEO::LEO::Platform>(driverHandles[i]));
        }
    }

    if (enableLEOFlag != 1 && NEO::LEO::platformsImpl) {
        bool leoEnabledForAnyDevice = false;
        for (const auto &platform : *NEO::LEO::platformsImpl) {
            for (const auto &device : platform->getDevices()) {
                auto &productHelper = device->getDevice().getRootDeviceEnvironment().getProductHelper();
                if (productHelper.isLEOSupported()) {
                    leoEnabledForAnyDevice = true;
                    break;
                }
            }
            if (leoEnabledForAnyDevice) {
                break;
            }
        }
        if (!leoEnabledForAnyDevice) {
            if (numPlatforms) {
                *numPlatforms = 0;
            }
            cl_int retVal = CL_SUCCESS;
            TRACING_EXIT(ClGetPlatformIDs, &retVal);
            return retVal;
        }
    }

    if (numPlatforms) {
        *numPlatforms = NEO::LEO::platformsImpl ? static_cast<cl_uint>(NEO::LEO::platformsImpl->size()) : 0u;
    }
    if (platforms && NEO::LEO::platformsImpl) {
        auto numPlatformsToReturn = std::min(static_cast<cl_uint>(NEO::LEO::platformsImpl->size()), numEntries);
        for (cl_uint i = 0; i < numPlatformsToReturn; ++i) {
            platforms[i] = (*NEO::LEO::platformsImpl)[i].get();
        }
    }

    cl_int retVal = CL_SUCCESS;
    TRACING_EXIT(ClGetPlatformIDs, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clIcdGetPlatformIDsKHR(cl_uint numEntries,
                                                       cl_platform_id *platforms,
                                                       cl_uint *numPlatforms) {
    TRACING_ENTER(ClIcdGetPlatformIDsKHR, &numEntries, &platforms, &numPlatforms);
    cl_int retVal = clGetPlatformIDs(numEntries, platforms, numPlatforms);
    TRACING_EXIT(ClIcdGetPlatformIDsKHR, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetPlatformInfo(cl_platform_id platform,
                                     cl_platform_info paramName,
                                     size_t paramValueSize,
                                     void *paramValue,
                                     size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetPlatformInfo, &platform, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pPlatform] = NEO::LEO::validateAndCast(std::make_tuple(platform));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetPlatformInfo, &retVal);
        return retVal;
    }

    cl_int ret = pPlatform->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetPlatformInfo, &ret);
    return ret;
}

cl_int CL_API_CALL clUnloadPlatformCompiler(cl_platform_id platform) {
    TRACING_ENTER(ClUnloadPlatformCompiler, &platform);
    auto [retVal, pPlatform] = NEO::LEO::validateAndCast(std::make_tuple(platform));
    TRACING_EXIT(ClUnloadPlatformCompiler, &retVal);
    return retVal;
}

cl_int CL_API_CALL clUnloadCompiler() {
    return CL_SUCCESS;
}

#define RETURN_FUNC_PTR_IF_EXIST(name)                                                    \
    {                                                                                     \
        if (!strcmp(funcName, #name)) {                                                   \
            void *ret = reinterpret_cast<void *>(name);                                   \
            TRACING_EXIT(ClGetExtensionFunctionAddress, reinterpret_cast<void **>(&ret)); \
            return ret;                                                                   \
        }                                                                                 \
    }
void *CL_API_CALL clGetExtensionFunctionAddress(const char *funcName) {
    TRACING_ENTER(ClGetExtensionFunctionAddress, &funcName);
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

    RETURN_FUNC_PTR_IF_EXIST(clCreateTracingHandleINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSetTracingPointINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clDestroyTracingHandleINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnableTracingINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clDisableTracingINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetTracingStateINTEL);

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
        TRACING_EXIT(ClGetExtensionFunctionAddress, &ret);
        return ret;
    }

    // SPIR-V support through the cl_khr_il_program extension
    RETURN_FUNC_PTR_IF_EXIST(clCreateProgramWithILKHR);
    RETURN_FUNC_PTR_IF_EXIST(clCreateCommandQueueWithPropertiesKHR);

    RETURN_FUNC_PTR_IF_EXIST(clSetProgramSpecializationConstant);

    RETURN_FUNC_PTR_IF_EXIST(clGetKernelSuggestedLocalWorkSize);
    RETURN_FUNC_PTR_IF_EXIST(clGetKernelSuggestedLocalWorkSizeKHR);
    ret = NEO::getAdditionalExtensionFunctionAddress(funcName);
    TRACING_EXIT(ClGetExtensionFunctionAddress, &ret);
    return ret;
}

// OpenCL 1.2
void *CL_API_CALL clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                                           const char *funcName) {
    TRACING_ENTER(ClGetExtensionFunctionAddressForPlatform, &platform, &funcName);
    void *ret = clGetExtensionFunctionAddress(funcName);
    TRACING_EXIT(ClGetExtensionFunctionAddressForPlatform, &ret);
    return ret;
}
