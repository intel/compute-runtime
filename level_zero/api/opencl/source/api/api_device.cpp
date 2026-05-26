/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/core/source/device/device.h"

#include "CL/cl.h"

cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform,
                                  cl_device_type deviceType,
                                  cl_uint numEntries,
                                  cl_device_id *devices,
                                  cl_uint *numDevices) {
    auto pPlatform = NEO::LEO::castToObject<NEO::LEO::Platform>(platform);
    if (!pPlatform) {
        pPlatform = (*NEO::LEO::platformsImpl)[0].get();
    }

    constexpr cl_device_type validType = CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_CPU |
                                         CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_DEFAULT |
                                         CL_DEVICE_TYPE_CUSTOM;

    if ((deviceType & validType) == 0) {
        return CL_INVALID_DEVICE_TYPE;
    }

    if ((devices == nullptr && numDevices == nullptr) ||
        (devices && numEntries == 0)) {
        return CL_INVALID_VALUE;
    }

    if (numDevices) {
        if (deviceType != CL_DEVICE_TYPE_GPU && deviceType != CL_DEVICE_TYPE_ALL && deviceType != CL_DEVICE_TYPE_DEFAULT) {
            *numDevices = 0;
            return CL_DEVICE_NOT_FOUND;
        } else {
            *numDevices = deviceType == CL_DEVICE_TYPE_DEFAULT ? 1u : static_cast<cl_uint>(pPlatform->getDevices().size());
        }
    }

    if (devices) {
        auto numDevicesToReturn = deviceType == CL_DEVICE_TYPE_DEFAULT ? 1u : std::min(static_cast<cl_uint>(pPlatform->getDevices().size()), numEntries);
        for (cl_uint i = 0; i < numDevicesToReturn; ++i) {
            devices[i] = pPlatform->getDevices()[i].get();
        }
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device,
                                   cl_device_info paramName,
                                   size_t paramValueSize,
                                   void *paramValue,
                                   size_t *paramValueSizeRet) {
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pDevice->getDeviceInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clCreateSubDevices(cl_device_id inDevice,
                                      const cl_device_partition_property *properties,
                                      cl_uint numDevices,
                                      cl_device_id *outDevices,
                                      cl_uint *numDevicesRet) {
    return CL_SUCCESS;
}

cl_int CL_API_CALL clRetainDevice(cl_device_id device) {
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pDevice->getIsSubdevice()) [[unlikely]] {
        pDevice->incRefApi();
    }
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseDevice(cl_device_id device) {
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pDevice->getIsSubdevice()) [[unlikely]] {
        pDevice->decRefApi();
    }
    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetDeviceAndHostTimer(cl_device_id device,
                                           cl_ulong *deviceTimestamp,
                                           cl_ulong *hostTimestamp) {
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device), std::make_tuple(static_cast<void *>(deviceTimestamp), static_cast<void *>(hostTimestamp)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (!pDevice->getDeviceAndHostTimer(static_cast<uint64_t *>(deviceTimestamp), static_cast<uint64_t *>(hostTimestamp))) [[unlikely]] {
        return CL_OUT_OF_RESOURCES;
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetHostTimer(cl_device_id device,
                                  cl_ulong *hostTimestamp) {
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device), std::make_tuple(static_cast<void *>(hostTimestamp)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (!pDevice->getHostTimer(static_cast<uint64_t *>(hostTimestamp))) [[unlikely]] {
        return CL_OUT_OF_RESOURCES;
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clAddCommentINTEL(cl_device_id device, const char *comment) {
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(
        std::make_tuple(device),
        std::make_tuple());
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto aubCenter = pDevice->getL0Object()->getNEODevice()->getRootDeviceEnvironment().aubCenter.get();

    if (!comment || (aubCenter && !aubCenter->getAubManager())) {
        return CL_INVALID_VALUE;
    }

    if (aubCenter) {
        aubCenter->getAubManager()->addComment(comment);
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetDeviceGlobalVariablePointerINTEL(
    cl_device_id device,
    cl_program program,
    const char *globalVariableName,
    size_t *globalVariableSizeRet,
    void **globalVariablePointerRet) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return L0ToClResultMapper(zeModuleGetGlobalPointer(pProgram->getModuleHandle(), globalVariableName, globalVariableSizeRet, globalVariablePointerRet));
}

cl_int CL_API_CALL clGetDeviceFunctionPointerINTEL(
    cl_device_id device,
    cl_program program,
    const char *functionName,
    cl_ulong *functionPointerRet) {
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return L0ToClResultMapper(zeModuleGetFunctionPointer(pProgram->getModuleHandle(), functionName, reinterpret_cast<void **>(functionPointerRet)));
}

CL_API_ENTRY cl_int CL_API_CALL
clSetPerformanceConfigurationINTEL(
    cl_device_id device,
    cl_uint count,
    cl_uint *offsets,
    cl_uint *values) {
    return CL_INVALID_OPERATION;
}
