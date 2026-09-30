/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/command_buffer/leo_command_buffer.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_get_info_status_mapper.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj.h"

#include <algorithm>
#include <limits>

namespace NEO {
namespace LEO {

CommandBuffer::CommandBuffer(Context *context, CommandQueue *commandQueue, ze_command_list_handle_t cmdListHandle, const cl_command_buffer_properties_khr *properties)
    : context(context), commandQueue(commandQueue), cmdListHandle(cmdListHandle) {
    context->incRefInternal();
    commandQueue->incRefInternal();

    this->storeProperties(properties);
}

CommandBuffer::~CommandBuffer() {
    for (auto queue : this->replayQueues) {
        queue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }
    zeCommandListDestroy(this->cmdListHandle);
    for (auto kernel : this->recordedKernels) {
        kernel->decRefInternal();
    }
    for (auto memObj : this->recordedMemObjs) {
        memObj->decRefInternal();
    }
    for (auto queue : this->replayQueues) {
        queue->decRefInternal();
    }
    this->commandQueue->decRefInternal();
    this->context->decRefInternal();
}

bool CommandBuffer::isSupported() {
    return debugManager.flags.EnableClKhrCommandBuffer.get() == 1;
}

cl_int CommandBuffer::finalize() {
    if (this->isFinalized()) {
        return CL_INVALID_OPERATION;
    }

    auto result = zeCommandListClose(this->cmdListHandle);
    if (result != ZE_RESULT_SUCCESS) {
        return L0ToClResultMapper(result);
    }

    this->state = CL_COMMAND_BUFFER_STATE_EXECUTABLE_KHR;
    return CL_SUCCESS;
}

cl_int CommandBuffer::validateCommand(cl_command_queue commandQueue, const cl_command_properties_khr *properties,
                                      cl_uint numSyncPointsInWaitList, const cl_sync_point_khr *syncPointWaitList,
                                      const cl_mutable_command_khr *mutableHandle) const {
    if (this->isFinalized()) {
        return CL_INVALID_OPERATION;
    }
    if (commandQueue != nullptr) {
        return CL_INVALID_COMMAND_QUEUE;
    }
    if ((properties != nullptr && *properties != 0) || (mutableHandle != nullptr)) {
        return CL_INVALID_VALUE;
    }
    if ((numSyncPointsInWaitList == 0u) != (syncPointWaitList == nullptr)) {
        return CL_INVALID_SYNC_POINT_WAIT_LIST_KHR;
    }
    for (cl_uint i = 0; i < numSyncPointsInWaitList; ++i) {
        if (syncPointWaitList[i] >= this->numRecordedCommands) {
            return CL_INVALID_SYNC_POINT_WAIT_LIST_KHR;
        }
    }
    return CL_SUCCESS;
}

cl_int CommandBuffer::recordCommand(ze_result_t appendResult, cl_sync_point_khr *syncPoint) {
    if (appendResult != ZE_RESULT_SUCCESS) {
        return L0ToClResultMapper(appendResult);
    }
    if (syncPoint != nullptr) {
        *syncPoint = this->numRecordedCommands;
    }
    ++this->numRecordedCommands;
    return CL_SUCCESS;
}

void CommandBuffer::keepAlive(Kernel *kernel) {
    kernel->incRefInternal();
    this->recordedKernels.push_back(kernel);
}

void CommandBuffer::keepAlive(MemObj *memObj) {
    memObj->incRefInternal();
    this->recordedMemObjs.push_back(memObj);
}

cl_int CommandBuffer::enqueue(CommandQueue *commandQueue, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    if (false == this->isFinalized()) {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COMMAND_BUFFER_KHR, commandQueue);

    auto lock = commandQueue->takeOwnership();
    auto result = zeCommandListImmediateAppendCommandListsExp(commandQueue->getL0Handle(), 1, &this->cmdListHandle,
                                                              hSignalEvent, waitEvents.size(), waitEvents.data());
    if (result == ZE_RESULT_SUCCESS && std::find(this->replayQueues.begin(), this->replayQueues.end(), commandQueue) == this->replayQueues.end()) {
        commandQueue->incRefInternal();
        this->replayQueues.push_back(commandQueue);
    }
    return L0ToClResultMapper(result);
}

cl_int CommandBuffer::getInfo(cl_command_buffer_info_khr paramName, size_t paramValueSize,
                              void *paramValue, size_t *paramValueSizeRet) {
    size_t valueSize = GetInfo::invalidSourceSize;
    const void *pValue = nullptr;

    cl_context clContext = this->context;
    cl_command_queue clCommandQueue = this->commandQueue;
    cl_uint numQueues = 1u;
    cl_uint refCount = 0;

    switch (paramName) {
    case CL_COMMAND_BUFFER_QUEUES_KHR:
        valueSize = sizeof(cl_command_queue);
        pValue = &clCommandQueue;
        break;

    case CL_COMMAND_BUFFER_NUM_QUEUES_KHR:
        valueSize = sizeof(numQueues);
        pValue = &numQueues;
        break;

    case CL_COMMAND_BUFFER_REFERENCE_COUNT_KHR:
        refCount = static_cast<cl_uint>(this->getReference());
        valueSize = sizeof(refCount);
        pValue = &refCount;
        break;

    case CL_COMMAND_BUFFER_STATE_KHR:
        valueSize = sizeof(this->state);
        pValue = &this->state;
        break;

    case CL_COMMAND_BUFFER_PROPERTIES_ARRAY_KHR:
        valueSize = this->bufferProperties.size() * sizeof(cl_command_buffer_properties_khr);
        pValue = this->bufferProperties.data();
        break;

    case CL_COMMAND_BUFFER_CONTEXT_KHR:
        valueSize = sizeof(cl_context);
        pValue = &clContext;
        break;

    default:
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pValue, valueSize);
    auto retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, valueSize, getInfoStatus);

    return retVal;
}

void CommandBuffer::storeProperties(const cl_command_buffer_properties_khr *properties) {
    if (properties != nullptr) {
        while (*properties != 0) {
            this->bufferProperties.push_back(*properties);
            ++properties;
            this->bufferProperties.push_back(*properties);
            ++properties;
        }
        this->bufferProperties.push_back(0u);
    }
}

} // namespace LEO
} // namespace NEO
