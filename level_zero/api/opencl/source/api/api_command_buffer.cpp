/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_buffer/leo_command_buffer.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

namespace {

// Only CL_COMMAND_BUFFER_FLAGS_KHR is defined by the extension. No flag value is supported yet:
// CL_COMMAND_BUFFER_SIMULTANEOUS_USE_KHR requires a per-replay sync point event set and
// CL_COMMAND_BUFFER_MUTABLE_KHR belongs to cl_khr_command_buffer_mutable_dispatch.
cl_int validateCommandBufferProperties(const cl_command_buffer_properties_khr *properties) {
    if (properties == nullptr) {
        return CL_SUCCESS;
    }

    bool foundFlags = false;
    while (*properties != 0) {
        if (*properties != CL_COMMAND_BUFFER_FLAGS_KHR) {
            return CL_INVALID_PROPERTY;
        }
        if (foundFlags) {
            return CL_INVALID_PROPERTY;
        }
        foundFlags = true;

        ++properties;
        if (*properties != 0) {
            return CL_INVALID_PROPERTY;
        }
        ++properties;
    }

    return CL_SUCCESS;
}

} // namespace

CL_API_ENTRY cl_command_buffer_khr CL_API_CALL clCreateCommandBufferKHR(
    cl_uint numQueues,
    const cl_command_queue *queues,
    const cl_command_buffer_properties_khr *properties,
    cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    if (false == NEO::LEO::CommandBuffer::isSupported()) {
        err.set(CL_INVALID_OPERATION);
        return nullptr;
    }

    // A single queue per command buffer. Multiple queues require cl_khr_command_buffer_multi_device.
    if ((numQueues != 1u) || (queues == nullptr)) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(queues[0]));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    retVal = validateCommandBufferProperties(properties);
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    auto contextHandle = pCommandQueue->getContext()->getL0ContextHandle();
    auto deviceHandle = NEO::LEO::ConvertTo::zeDeviceHandle(pCommandQueue->getDevice());

    // Mirror the ordering and copy-offload behaviour of the queue the buffer is created with, so a
    // recorded command behaves the same as the equivalent enqueue on that queue. These are fixed
    // here, so a replay onto a different queue does not take on that queue's ordering.
    ze_command_list_flags_t cmdListFlags = ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT;
    if (false == pCommandQueue->isOutOfOrder()) {
        cmdListFlags |= ZE_COMMAND_LIST_FLAG_IN_ORDER;
    }

    // Recorded commands are placed on the default compute engine group. Once command recording is
    // implemented this has to follow the engine group of the queue the buffer is replayed onto.
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC,
                                          nullptr,
                                          0u,
                                          cmdListFlags};

    ze_command_list_handle_t cmdListHandle = nullptr;
    auto result = zeCommandListCreate(contextHandle, deviceHandle, &cmdListDesc, &cmdListHandle);
    if (result != ZE_RESULT_SUCCESS) {
        err.set(L0ToClResultMapper(result));
        return nullptr;
    }

    return new NEO::LEO::CommandBuffer(pCommandQueue->getContext(), pCommandQueue, cmdListHandle, properties);
}

CL_API_ENTRY cl_int CL_API_CALL clFinalizeCommandBufferKHR(
    cl_command_buffer_khr commandBuffer) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    return pCommandBuffer->finalize();
}

CL_API_ENTRY cl_int CL_API_CALL clRetainCommandBufferKHR(
    cl_command_buffer_khr commandBuffer) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pCommandBuffer->incRefApi();
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clReleaseCommandBufferKHR(
    cl_command_buffer_khr commandBuffer) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pCommandBuffer->decRefApi();
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueCommandBufferKHR(
    cl_uint numQueues,
    cl_command_queue *queues,
    cl_command_buffer_khr commandBuffer,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer),
                                                              std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    // A single queue per replay. Multiple queues require cl_khr_command_buffer_multi_device.
    if (numQueues > 1u) {
        return CL_INVALID_VALUE;
    }
    // numQueues and queues have to agree: either both are empty or both are given.
    if ((numQueues == 0u) != (queues == nullptr)) {
        return CL_INVALID_VALUE;
    }

    // With no queue given the buffer replays onto the queue it was created with. A given queue may
    // be a different one, as long as it is on the same device and in the same context.
    auto pTargetQueue = pCommandBuffer->getCommandQueue();
    if (numQueues == 1u) {
        auto [queueRetVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(queues[0]));
        if (queueRetVal != CL_SUCCESS) [[unlikely]] {
            return queueRetVal;
        }
        if (pCommandQueue->getContext() != pTargetQueue->getContext()) {
            return CL_INVALID_CONTEXT;
        }
        if (pCommandQueue->getDevice() != pTargetQueue->getDevice()) {
            return CL_INVALID_DEVICE;
        }
        pTargetQueue = pCommandQueue;
    }

    auto lock = pCommandBuffer->takeOwnership();
    return pCommandBuffer->enqueue(pTargetQueue, numEventsInWaitList, eventWaitList, event);
}

CL_API_ENTRY cl_int CL_API_CALL clGetCommandBufferInfoKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_buffer_info_khr paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    return pCommandBuffer->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}
