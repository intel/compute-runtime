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

namespace NEO {
namespace LEO {

CommandBuffer::CommandBuffer(Context *context, CommandQueue *commandQueue, ze_command_list_handle_t cmdListHandle, const cl_command_buffer_properties_khr *properties)
    : context(context), commandQueue(commandQueue), cmdListHandle(cmdListHandle) {
    context->incRefInternal();
    commandQueue->incRefInternal();

    this->storeProperties(properties);
}

CommandBuffer::~CommandBuffer() {
    zeCommandListDestroy(this->cmdListHandle);
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

cl_int CommandBuffer::enqueue(CommandQueue *commandQueue, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    if (false == this->isFinalized()) {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COMMAND_BUFFER_KHR, commandQueue);

    auto lock = commandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListImmediateAppendCommandListsExp(commandQueue->getL0Handle(), 1, &this->cmdListHandle,
                                                                          hSignalEvent, waitEvents.size(), waitEvents.data()));
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
