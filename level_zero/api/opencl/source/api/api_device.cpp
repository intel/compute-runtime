/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"
#include "level_zero/core/source/device/device.h"

#include "CL/cl.h"

cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform,
                                  cl_device_type deviceType,
                                  cl_uint numEntries,
                                  cl_device_id *devices,
                                  cl_uint *numDevices) {
    TRACING_ENTER(ClGetDeviceIDs, &platform, &deviceType, &numEntries, &devices, &numDevices);
    auto pPlatform = NEO::LEO::castToObject<NEO::LEO::Platform>(platform);
    if (!pPlatform) {
        pPlatform = (*NEO::LEO::platformsImpl)[0].get();
    }

    constexpr cl_device_type validType = CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_CPU |
                                         CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_DEFAULT |
                                         CL_DEVICE_TYPE_CUSTOM;

    if ((deviceType & validType) == 0) {
        cl_int tracingRetVal = CL_INVALID_DEVICE_TYPE;
        TRACING_EXIT(ClGetDeviceIDs, &tracingRetVal);
        return tracingRetVal;
    }

    if ((devices == nullptr && numDevices == nullptr) ||
        (devices && numEntries == 0)) {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClGetDeviceIDs, &tracingRetVal);
        return tracingRetVal;
    }

    if (numDevices) {
        if (deviceType != CL_DEVICE_TYPE_GPU && deviceType != CL_DEVICE_TYPE_ALL && deviceType != CL_DEVICE_TYPE_DEFAULT) {
            *numDevices = 0;
            cl_int tracingRetVal = CL_DEVICE_NOT_FOUND;
            TRACING_EXIT(ClGetDeviceIDs, &tracingRetVal);
            return tracingRetVal;
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

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClGetDeviceIDs, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device,
                                   cl_device_info paramName,
                                   size_t paramValueSize,
                                   void *paramValue,
                                   size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetDeviceInfo, &device, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetDeviceInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pDevice->getDeviceInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetDeviceInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clCreateSubDevices(cl_device_id inDevice,
                                      const cl_device_partition_property *properties,
                                      cl_uint numDevices,
                                      cl_device_id *outDevices,
                                      cl_uint *numDevicesRet) {
    TRACING_ENTER(ClCreateSubDevices, &inDevice, &properties, &numDevices, &outDevices, &numDevicesRet);
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClCreateSubDevices, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clRetainDevice(cl_device_id device) {
    TRACING_ENTER(ClRetainDevice, &device);
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClRetainDevice, &retVal);
        return retVal;
    }

    if (pDevice->getIsSubdevice()) [[unlikely]] {
        pDevice->incRefApi();
    }
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClRetainDevice, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clReleaseDevice(cl_device_id device) {
    TRACING_ENTER(ClReleaseDevice, &device);
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClReleaseDevice, &retVal);
        return retVal;
    }

    if (pDevice->getIsSubdevice()) [[unlikely]] {
        pDevice->decRefApi();
    }
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClReleaseDevice, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetDeviceAndHostTimer(cl_device_id device,
                                           cl_ulong *deviceTimestamp,
                                           cl_ulong *hostTimestamp) {
    TRACING_ENTER(ClGetDeviceAndHostTimer, &device, &deviceTimestamp, &hostTimestamp);
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device), std::make_tuple(static_cast<void *>(deviceTimestamp), static_cast<void *>(hostTimestamp)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetDeviceAndHostTimer, &retVal);
        return retVal;
    }

    if (!pDevice->getDeviceAndHostTimer(static_cast<uint64_t *>(deviceTimestamp), static_cast<uint64_t *>(hostTimestamp))) [[unlikely]] {
        cl_int tracingRetVal = CL_OUT_OF_RESOURCES;
        TRACING_EXIT(ClGetDeviceAndHostTimer, &tracingRetVal);
        return tracingRetVal;
    }

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClGetDeviceAndHostTimer, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetHostTimer(cl_device_id device,
                                  cl_ulong *hostTimestamp) {
    TRACING_ENTER(ClGetHostTimer, &device, &hostTimestamp);
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(device), std::make_tuple(static_cast<void *>(hostTimestamp)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetHostTimer, &retVal);
        return retVal;
    }

    if (!pDevice->getHostTimer(static_cast<uint64_t *>(hostTimestamp))) [[unlikely]] {
        cl_int tracingRetVal = CL_OUT_OF_RESOURCES;
        TRACING_EXIT(ClGetHostTimer, &tracingRetVal);
        return tracingRetVal;
    }

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClGetHostTimer, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clAddCommentINTEL(cl_device_id device, const char *comment) {
    TRACING_ENTER(ClAddCommentINTEL, &device, &comment);
    auto [retVal, pDevice] = NEO::LEO::validateAndCast(
        std::make_tuple(device),
        std::make_tuple());
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClAddCommentINTEL, &retVal);
        return retVal;
    }

    auto aubCenter = pDevice->getL0Object()->getNEODevice()->getRootDeviceEnvironment().aubCenter.get();

    if (!comment || (aubCenter && !aubCenter->getAubManager())) {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClAddCommentINTEL, &tracingRetVal);
        return tracingRetVal;
    }

    if (aubCenter) {
        aubCenter->getAubManager()->addComment(comment);
    }

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClAddCommentINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetDeviceGlobalVariablePointerINTEL(
    cl_device_id device,
    cl_program program,
    const char *globalVariableName,
    size_t *globalVariableSizeRet,
    void **globalVariablePointerRet) {
    TRACING_ENTER(ClGetDeviceGlobalVariablePointerINTEL, &device, &program, &globalVariableName, &globalVariableSizeRet, &globalVariablePointerRet);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetDeviceGlobalVariablePointerINTEL, &retVal);
        return retVal;
    }

    auto pDevice = NEO::LEO::castToObject<NEO::LEO::ClDevice>(device);
    auto moduleHandle = pDevice ? pProgram->getModuleHandle(pDevice->getRootDeviceIndex()) : pProgram->getModuleHandle();
    cl_int tracingRetVal = L0ToClResultMapper(zeModuleGetGlobalPointer(moduleHandle, globalVariableName, globalVariableSizeRet, globalVariablePointerRet));
    TRACING_EXIT(ClGetDeviceGlobalVariablePointerINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetDeviceFunctionPointerINTEL(
    cl_device_id device,
    cl_program program,
    const char *functionName,
    cl_ulong *functionPointerRet) {
    TRACING_ENTER(ClGetDeviceFunctionPointerINTEL, &device, &program, &functionName, &functionPointerRet);
    auto [retVal, pProgram] = NEO::LEO::validateAndCast(std::make_tuple(program));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetDeviceFunctionPointerINTEL, &retVal);
        return retVal;
    }

    auto pDevice = NEO::LEO::castToObject<NEO::LEO::ClDevice>(device);
    auto moduleHandle = pDevice ? pProgram->getModuleHandle(pDevice->getRootDeviceIndex()) : pProgram->getModuleHandle();
    cl_int tracingRetVal = L0ToClResultMapper(zeModuleGetFunctionPointer(moduleHandle, functionName, reinterpret_cast<void **>(functionPointerRet)));
    TRACING_EXIT(ClGetDeviceFunctionPointerINTEL, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL
clSetPerformanceConfigurationINTEL(
    cl_device_id device,
    cl_uint count,
    cl_uint *offsets,
    cl_uint *values) {
    return CL_INVALID_OPERATION;
}
