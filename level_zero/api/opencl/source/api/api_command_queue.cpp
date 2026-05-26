/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_command_queue CL_API_CALL clCreateCommandQueue(cl_context context,
                                                  cl_device_id device,
                                                  const cl_command_queue_properties properties,
                                                  cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(context, device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    if (properties &
        ~(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE)) [[unlikely]] {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }
    cl_queue_properties queueProperties[3] = {CL_QUEUE_PROPERTIES, properties, 0};
    return clCreateCommandQueueWithProperties(context, device, queueProperties, errcodeRet);
}

cl_command_queue CL_API_CALL clCreateCommandQueueWithProperties(cl_context context,
                                                                cl_device_id device,
                                                                const cl_queue_properties *properties,
                                                                cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(context, device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    if (properties) [[likely]] {
        auto propertiesAddress = properties;
        while (*propertiesAddress != 0) {
            switch (*propertiesAddress) {
            case CL_QUEUE_PROPERTIES:
            case CL_QUEUE_SIZE:
            case CL_QUEUE_PRIORITY_KHR:
            case CL_QUEUE_THROTTLE_KHR:
            case CL_QUEUE_SLICE_COUNT_INTEL:
            case CL_QUEUE_FAMILY_INTEL:
            case CL_QUEUE_INDEX_INTEL:
            case CL_QUEUE_MDAPI_PROPERTIES_INTEL:
            case CL_QUEUE_MDAPI_CONFIGURATION_INTEL:
            case CL_L0_IMMEDIATE_CMD_LIST_HANDLE:
                break;
            default:
                err.set(CL_INVALID_VALUE);
                return nullptr;
            }
            propertiesAddress += 2;
        }

        auto commandQueueProperties = NEO::LEO::CommandQueue::getCmdQueueProperties<cl_command_queue_properties>(properties);
        if (commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE)) {
            if (!(commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE))) {
                err.set(CL_INVALID_VALUE);
                return nullptr;
            }
            err.set(CL_INVALID_QUEUE_PROPERTIES);
            return nullptr;
        }
    }

    bool inputCmdList = false;
    auto inputCmdListHandle = NEO::LEO::CommandQueue::getCmdQueueProperties<uintptr_t>(properties, CL_L0_IMMEDIATE_CMD_LIST_HANDLE, &inputCmdList);
    if (inputCmdList) {
        if (errcodeRet) {
            *errcodeRet = CL_SUCCESS;
        }
        return new NEO::LEO::CommandQueue(pContext, pDevice, properties, reinterpret_cast<ze_command_list_handle_t>(inputCmdListHandle));
    }

    return new NEO::LEO::CommandQueue(pContext, pDevice, properties);
}

cl_command_queue CL_API_CALL clCreateCommandQueueWithPropertiesKHR(cl_context context,
                                                                   cl_device_id device,
                                                                   const cl_queue_properties_khr *properties,
                                                                   cl_int *errcodeRet) {
    return clCreateCommandQueueWithProperties(context, device, properties, errcodeRet);
}

cl_int CL_API_CALL clRetainCommandQueue(cl_command_queue commandQueue) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pCommandQueue->incRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseCommandQueue(cl_command_queue commandQueue) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pCommandQueue->decRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetCommandQueueInfo(cl_command_queue commandQueue,
                                         cl_command_queue_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pCommandQueue->getCmdQInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clSetCommandQueueProperty(cl_command_queue commandQueue,
                                             cl_command_queue_properties properties,
                                             cl_bool enable,
                                             cl_command_queue_properties *oldProperties) {
    return CL_INVALID_OPERATION;
}

cl_int CL_API_CALL clFlush(cl_command_queue commandQueue) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clFinish(cl_command_queue commandQueue) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return L0ToClResultMapper(zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max()));
}

CL_API_ENTRY cl_command_queue CL_API_CALL
clCreatePerfCountersCommandQueueINTEL(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_uint configuration,
    cl_int *errcodeRet) {
    return nullptr;
}
