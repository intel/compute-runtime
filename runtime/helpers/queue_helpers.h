/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/get_info.h"

namespace OCLRT {
template <typename QueueType>
void retainQueue(cl_command_queue commandQueue, cl_int &retVal) {
    using BaseType = typename QueueType::BaseType;
    auto queue = castToObject<QueueType>(static_cast<BaseType *>(commandQueue));
    if (queue) {
        queue->retain();
        retVal = CL_SUCCESS;
    }
}

template <typename QueueType>
void releaseQueue(cl_command_queue commandQueue, cl_int &retVal) {
    using BaseType = typename QueueType::BaseType;
    auto queue = castToObject<QueueType>(static_cast<BaseType *>(commandQueue));
    if (queue) {
        queue->release();
        retVal = CL_SUCCESS;
    }
}

template <typename QueueType>
cl_int getQueueInfo(QueueType *queue,
                    cl_command_queue_info paramName,
                    size_t paramValueSize,
                    void *paramValue,
                    size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;

    GetInfoHelper getInfoHelper(paramValue, paramValueSize, paramValueSizeRet, &retVal);

    switch (paramName) {
    case CL_QUEUE_CONTEXT:
        getInfoHelper.set<cl_context>(&queue->getContext());
        break;
    case CL_QUEUE_DEVICE:
        getInfoHelper.set<cl_device_id>(&queue->getDevice());
        break;
    case CL_QUEUE_REFERENCE_COUNT:
        getInfoHelper.set<cl_int>(queue->getReference());
        break;
    case CL_QUEUE_PROPERTIES:
        getInfoHelper.set<cl_command_queue_properties>(queue->getCommandQueueProperties());
        break;
    case CL_QUEUE_DEVICE_DEFAULT:
        getInfoHelper.set<cl_command_queue>(queue->getContext().getDefaultDeviceQueue());
        break;
    case CL_QUEUE_SIZE:
        if (std::is_same<QueueType, class DeviceQueue>::value) {
            auto devQ = reinterpret_cast<DeviceQueue *>(queue);
            getInfoHelper.set<cl_uint>(devQ->getQueueSize());
            break;
        }
        retVal = CL_INVALID_VALUE;
        break;
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    return retVal;
}

template <typename QueueType>
void getQueueInfo(cl_command_queue commandQueue,
                  cl_command_queue_info paramName,
                  size_t paramValueSize,
                  void *paramValue,
                  size_t *paramValueSizeRet,
                  cl_int &retVal) {
    using BaseType = typename QueueType::BaseType;
    auto queue = castToObject<QueueType>(static_cast<BaseType *>(commandQueue));
    if (queue) {
        retVal = getQueueInfo<QueueType>(queue, paramName, paramValueSize, paramValue, paramValueSizeRet);
    }
}

template <typename returnType>
returnType getCmdQueueProperties(const cl_queue_properties *properties,
                                 cl_queue_properties propertyName = CL_QUEUE_PROPERTIES) {
    returnType retVal = 0;

    while (properties != nullptr && *properties != 0) {
        if (*properties == propertyName) {
            ++properties;
            retVal = static_cast<returnType>(*properties);
            return retVal;
        }
        ++properties;
    }
    return retVal;
}
bool processExtraTokens(Device *&device, const cl_queue_properties *property);
} // namespace OCLRT
