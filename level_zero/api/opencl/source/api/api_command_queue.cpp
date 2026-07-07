/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_command_queue CL_API_CALL clCreateCommandQueue(cl_context context,
                                                  cl_device_id device,
                                                  const cl_command_queue_properties properties,
                                                  cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateCommandQueue, &context, &device, &properties, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(context, device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        cl_command_queue tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateCommandQueue, &tracingRetVal);
        return tracingRetVal;
    }

    if (properties &
        ~(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE)) [[unlikely]] {
        err.set(CL_INVALID_VALUE);
        cl_command_queue tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateCommandQueue, &tracingRetVal);
        return tracingRetVal;
    }
    cl_queue_properties queueProperties[3] = {CL_QUEUE_PROPERTIES, properties, 0};
    cl_command_queue tracingRetVal = clCreateCommandQueueWithProperties(context, device, queueProperties, errcodeRet);
    TRACING_EXIT(ClCreateCommandQueue, &tracingRetVal);
    return tracingRetVal;
}

cl_command_queue CL_API_CALL clCreateCommandQueueWithProperties(cl_context context,
                                                                cl_device_id device,
                                                                const cl_queue_properties *properties,
                                                                cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateCommandQueueWithProperties, &context, &device, &properties, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(context, device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        cl_command_queue tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &tracingRetVal);
        return tracingRetVal;
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
                cl_command_queue tracingRetVal = nullptr;
                TRACING_EXIT(ClCreateCommandQueueWithProperties, &tracingRetVal);
                return tracingRetVal;
            }
            propertiesAddress += 2;
        }

        auto commandQueueProperties = NEO::LEO::CommandQueue::getCmdQueueProperties<cl_command_queue_properties>(properties);
        if (commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE)) {
            if (!(commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE))) {
                err.set(CL_INVALID_VALUE);
                cl_command_queue tracingRetVal = nullptr;
                TRACING_EXIT(ClCreateCommandQueueWithProperties, &tracingRetVal);
                return tracingRetVal;
            }
            err.set(CL_INVALID_QUEUE_PROPERTIES);
            cl_command_queue tracingRetVal = nullptr;
            TRACING_EXIT(ClCreateCommandQueueWithProperties, &tracingRetVal);
            return tracingRetVal;
        }
    }

    bool inputCmdList = false;
    auto inputCmdListHandle = NEO::LEO::CommandQueue::getCmdQueueProperties<uintptr_t>(properties, CL_L0_IMMEDIATE_CMD_LIST_HANDLE, &inputCmdList);
    if (inputCmdList) {
        if (errcodeRet) {
            *errcodeRet = CL_SUCCESS;
        }
        cl_command_queue tracingRetVal = new NEO::LEO::CommandQueue(pContext, pDevice, properties, reinterpret_cast<ze_command_list_handle_t>(inputCmdListHandle));
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &tracingRetVal);
        return tracingRetVal;
    }

    cl_command_queue tracingRetVal = new NEO::LEO::CommandQueue(pContext, pDevice, properties);
    TRACING_EXIT(ClCreateCommandQueueWithProperties, &tracingRetVal);
    return tracingRetVal;
}

cl_command_queue CL_API_CALL clCreateCommandQueueWithPropertiesKHR(cl_context context,
                                                                   cl_device_id device,
                                                                   const cl_queue_properties_khr *properties,
                                                                   cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateCommandQueueWithPropertiesKHR, &context, &device, &properties, &errcodeRet);
    cl_command_queue tracingRetVal = clCreateCommandQueueWithProperties(context, device, properties, errcodeRet);
    TRACING_EXIT(ClCreateCommandQueueWithPropertiesKHR, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clRetainCommandQueue(cl_command_queue commandQueue) {
    TRACING_ENTER(ClRetainCommandQueue, &commandQueue);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClRetainCommandQueue, &retVal);
        return retVal;
    }

    pCommandQueue->incRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClRetainCommandQueue, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clReleaseCommandQueue(cl_command_queue commandQueue) {
    TRACING_ENTER(ClReleaseCommandQueue, &commandQueue);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClReleaseCommandQueue, &retVal);
        return retVal;
    }

    if (pCommandQueue->getReference() == 1 && pCommandQueue->hasDependencies()) {
        pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    pCommandQueue->decRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClReleaseCommandQueue, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetCommandQueueInfo(cl_command_queue commandQueue,
                                         cl_command_queue_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetCommandQueueInfo, &commandQueue, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetCommandQueueInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pCommandQueue->getCmdQInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetCommandQueueInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetCommandQueueProperty(cl_command_queue commandQueue,
                                             cl_command_queue_properties properties,
                                             cl_bool enable,
                                             cl_command_queue_properties *oldProperties) {
    TRACING_ENTER(ClSetCommandQueueProperty, &commandQueue, &properties, &enable, &oldProperties);
    cl_int tracingRetVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClSetCommandQueueProperty, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clFlush(cl_command_queue commandQueue) {
    TRACING_ENTER(ClFlush, &commandQueue);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClFlush, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClFlush, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clFinish(cl_command_queue commandQueue) {
    TRACING_ENTER(ClFinish, &commandQueue);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClFinish, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = L0ToClResultMapper(pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    TRACING_EXIT(ClFinish, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_command_queue CL_API_CALL
clCreatePerfCountersCommandQueueINTEL(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_uint configuration,
    cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(context, device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    if (!pDevice->getHardwareInfo().capabilityTable.instrumentationEnabled) {
        err.set(CL_INVALID_DEVICE);
        return nullptr;
    }

    if ((properties & CL_QUEUE_PROFILING_ENABLE) == 0 ||
        (properties & CL_QUEUE_ON_DEVICE) != 0 ||
        (properties & CL_QUEUE_ON_DEVICE_DEFAULT) != 0) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        return nullptr;
    }

    if (configuration != 0) {
        err.set(CL_INVALID_OPERATION);
        return nullptr;
    }

    cl_command_queue commandQueue = clCreateCommandQueue(context, device, properties, errcodeRet);
    if (commandQueue != nullptr) {
        auto pCommandQueue = NEO::LEO::castToObject<NEO::LEO::CommandQueue>(commandQueue);

        if (!pCommandQueue->setPerfCountersEnabled()) {
            clReleaseCommandQueue(commandQueue);
            commandQueue = nullptr;
            err.set(CL_OUT_OF_RESOURCES);
        }
    }
    return commandQueue;
}
