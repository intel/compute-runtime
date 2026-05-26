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
#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_context CL_API_CALL clCreateContext(const cl_context_properties *properties,
                                       cl_uint numDevices,
                                       const cl_device_id *devices,
                                       void(CL_CALLBACK *funcNotify)(const char *, const void *,
                                                                     size_t, void *),
                                       void *userData,
                                       cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    if (numDevices == 0 || !devices) {
        err.set(CL_INVALID_DEVICE);
        return nullptr;
    }

    if (funcNotify == nullptr && userData != nullptr) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    std::vector<ze_device_handle_t> l0Devices(numDevices);
    for (int i = 0; i < std::ssize(l0Devices); ++i) {
        l0Devices[i] = NEO::LEO::ConvertTo::zeDeviceHandle(devices[i]);
    }
    ze_driver_handle_t l0Driver = L0::Device::fromHandle(l0Devices[0])->getDriverHandle();

    bool inputContext = false;
    auto inputContextHandle = NEO::LEO::Context::getContextProperties<uintptr_t>(properties, CL_L0_CONTEXT_HANDLE, &inputContext);

    ze_context_handle_t context{};
    cl_int ret = CL_SUCCESS;

    if (inputContext) {
        context = reinterpret_cast<ze_context_handle_t>(inputContextHandle);
    } else {
        ze_context_desc_t contextDesc{ZE_STRUCTURE_TYPE_CONTEXT_DESC};
        ret = L0ToClResultMapper(zeContextCreateEx(l0Driver, &contextDesc, numDevices, l0Devices.data(), &context));
    }

    if (ret != CL_SUCCESS) {
        err.set(ret);
        return nullptr;
    }

    auto pContext = new NEO::LEO::Context(properties, context, numDevices, devices, inputContext);
    ret = pContext->initialize();

    err.set(ret);

    if (ret != CL_SUCCESS) {
        delete pContext;
        return nullptr;
    }

    return pContext;
}

cl_context CL_API_CALL clCreateContextFromType(const cl_context_properties *properties,
                                               cl_device_type deviceType,
                                               void(CL_CALLBACK *funcNotify)(const char *, const void *,
                                                                             size_t, void *),
                                               void *userData,
                                               cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    cl_uint numDevices = 0;
    auto retVal = clGetDeviceIDs(nullptr, deviceType, 0, nullptr, &numDevices);
    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        return nullptr;
    }
    std::vector<cl_device_id> devices(numDevices, nullptr);
    retVal = clGetDeviceIDs(nullptr, deviceType, numDevices, devices.data(), nullptr);
    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        return nullptr;
    }

    return clCreateContext(properties, numDevices, devices.data(), funcNotify, userData, errcodeRet);
}

cl_int CL_API_CALL clRetainContext(cl_context context) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pContext->incRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseContext(cl_context context) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pContext->decRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetContextInfo(cl_context context,
                                    cl_context_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pContext->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clSetContextDestructorCallback(cl_context context,
                                                  void(CL_CALLBACK *pfnNotify)(cl_context, void *),
                                                  void *userData) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context), std::make_tuple(reinterpret_cast<void *>(pfnNotify)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }
    pContext->registerCallback(pfnNotify, userData);
    return CL_SUCCESS;
}

cl_int CL_API_CALL clSetDefaultDeviceCommandQueue(cl_context context,
                                                  cl_device_id device,
                                                  cl_command_queue commandQueue) {
    return CL_INVALID_OPERATION;
}
