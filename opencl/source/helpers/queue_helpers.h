/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/get_info_status_mapper.h"

namespace NEO {

inline void releaseVirtualEvent(CommandQueue &commandQueue) {
    if (commandQueue.getRefApiCount() == 1) {
        commandQueue.releaseVirtualEvent();
    }
}

inline bool isCommandWithoutKernel(uint32_t commandType) {
    return ((commandType == CL_COMMAND_BARRIER) ||
            (commandType == CL_COMMAND_MARKER) ||
            (commandType == CL_COMMAND_MIGRATE_MEM_OBJECTS) ||
            (commandType == CL_COMMAND_RESOURCE_BARRIER) ||
            (commandType == CL_COMMAND_SVM_FREE) ||
            (commandType == CL_COMMAND_SVM_MAP) ||
            (commandType == CL_COMMAND_SVM_MIGRATE_MEM) ||
            (commandType == CL_COMMAND_SVM_UNMAP));
}

inline bool isFlushForProfilingRequired(uint32_t commandType) {
    return (commandType == CL_COMMAND_BARRIER || commandType == CL_COMMAND_MARKER);
}

inline void retainQueue(cl_command_queue commandQueue, cl_int &retVal) {
    using BaseType = typename CommandQueue::BaseType;
    auto queue = castToObject<CommandQueue>(static_cast<BaseType *>(commandQueue));
    if (queue) {
        queue->retain();
        retVal = CL_SUCCESS;
    }
}

inline void releaseQueue(cl_command_queue commandQueue, cl_int &retVal) {
    using BaseType = typename CommandQueue::BaseType;
    auto queue = castToObject<CommandQueue>(static_cast<BaseType *>(commandQueue));
    if (queue) {
        queue->flush();
        releaseVirtualEvent(*queue);

        if (queue->waitOnDestructionNeeded()) {
            queue->waitForAllEngines(false, nullptr, true, false);
        }
        queue->release();
        retVal = CL_SUCCESS;
    }
}

inline void getHostQueueInfo(CommandQueue *queue, cl_command_queue_info paramName, GetInfoHelper &getInfoHelper, cl_int &retVal) {
    switch (paramName) {
    case CL_QUEUE_FAMILY_INTEL:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_uint>(queue->getQueueFamilyIndex()));
        break;
    case CL_QUEUE_INDEX_INTEL:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_uint>(queue->getQueueIndexWithinFamily()));
        break;
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }
}

inline cl_int getQueueInfo(CommandQueue *queue,
                           cl_command_queue_info paramName,
                           size_t paramValueSize,
                           void *paramValue,
                           size_t *paramValueSizeRet) {

    cl_int retVal = CL_SUCCESS;
    GetInfoHelper getInfoHelper(paramValue, paramValueSize, paramValueSizeRet);

    switch (paramName) {
    case CL_QUEUE_CONTEXT:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_context>(queue->getContextPtr()));
        break;
    case CL_QUEUE_DEVICE: {
        Device &device = queue->getDevice();
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_device_id>(device.getSpecializedDevice<ClDevice>()));
        break;
    }
    case CL_QUEUE_REFERENCE_COUNT:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_int>(queue->getReference()));
        break;
    case CL_QUEUE_PROPERTIES:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_command_queue_properties>(queue->getCommandQueueProperties()));
        break;
    case CL_QUEUE_DEVICE_DEFAULT:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_command_queue>(nullptr));
        break;
    case CL_QUEUE_SIZE:
        retVal = CL_INVALID_COMMAND_QUEUE;
        break;
    case CL_QUEUE_PROPERTIES_ARRAY: {
        auto &propertiesVector = queue->getPropertiesVector();
        auto source = propertiesVector.data();
        auto sourceSize = propertiesVector.size() * sizeof(cl_queue_properties);
        auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, source, sourceSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
        GetInfo::setParamValueReturnSize(paramValueSizeRet, sourceSize, getInfoStatus);
        break;
    }
    default:
        getHostQueueInfo(queue, paramName, getInfoHelper, retVal);
        break;
    }

    return retVal;
}

inline void getQueueInfo(cl_command_queue commandQueue,
                         cl_command_queue_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet,
                         cl_int &retVal) {
    using BaseType = typename CommandQueue::BaseType;
    auto queue = castToObject<CommandQueue>(static_cast<BaseType *>(commandQueue));
    if (queue) {
        retVal = getQueueInfo(queue, paramName, paramValueSize, paramValue, paramValueSizeRet);
    }
}

template <typename ReturnType>
ReturnType getCmdQueueProperties(const cl_queue_properties *properties,
                                 cl_queue_properties propertyName = CL_QUEUE_PROPERTIES,
                                 bool *foundValue = nullptr) {
    if (properties != nullptr) {
        while (*properties != 0) {
            if (*properties == propertyName) {
                if (foundValue) {
                    *foundValue = true;
                }
                return static_cast<ReturnType>(*(properties + 1));
            }
            properties += 2;
        }
    }

    if (foundValue) {
        *foundValue = false;
    }
    return 0;
}
} // namespace NEO
