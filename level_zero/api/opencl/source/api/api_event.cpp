/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/internal/l0_cmdlist.h"
#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/event/event.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/tracing/tracing_notify.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_int CL_API_CALL clWaitForEvents(cl_uint numEvents,
                                   const cl_event *eventList) {
    TRACING_ENTER(ClWaitForEvents, &numEvents, &eventList);
    auto [retVal] = NEO::LEO::validateAndCast(std::make_tuple(), std::make_tuple(NEO::LEO::EventWaitList{eventList, numEvents}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClWaitForEvents, &retVal);
        return retVal;
    }

    for (cl_uint i = 0; i < numEvents; ++i) {
        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(eventList[i]);
        if (pEvent->getContext()->isTerminated()) {
            cl_int tracingRetVal = CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
            TRACING_EXIT(ClWaitForEvents, &tracingRetVal);
            return tracingRetVal;
        }

        auto ret = pEvent->wait();
        if (ret != ZE_RESULT_SUCCESS) {
            cl_int tracingRetVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClWaitForEvents, &tracingRetVal);
            return tracingRetVal;
        }
    }
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClWaitForEvents, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetEventInfo(cl_event event,
                                  cl_event_info paramName,
                                  size_t paramValueSize,
                                  void *paramValue,
                                  size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetEventInfo, &event, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pEvent->getEventInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetEventInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_event CL_API_CALL clCreateUserEvent(cl_context context,
                                       cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateUserEvent, &context, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        cl_event tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateUserEvent, &tracingRetVal);
        return tracingRetVal;
    }
    cl_event tracingRetVal = new NEO::LEO::Event(pContext);
    TRACING_EXIT(ClCreateUserEvent, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clRetainEvent(cl_event event) {
    TRACING_ENTER(ClRetainEvent, &event);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClRetainEvent, &retVal);
        return retVal;
    }

    pEvent->incRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClRetainEvent, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clReleaseEvent(cl_event event) {
    TRACING_ENTER(ClReleaseEvent, &event);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClReleaseEvent, &retVal);
        return retVal;
    }

    pEvent->decRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClReleaseEvent, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetUserEventStatus(cl_event event,
                                        cl_int executionStatus) {
    TRACING_ENTER(ClSetUserEventStatus, &event, &executionStatus);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClSetUserEventStatus, &retVal);
        return retVal;
    }

    if (executionStatus != CL_COMPLETE) {
        pEvent->getContext()->terminateExecution();
    }
    cl_int tracingRetVal = pEvent->signal();
    TRACING_EXIT(ClSetUserEventStatus, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetEventCallback(cl_event event,
                                      cl_int commandExecCallbackType,
                                      void(CL_CALLBACK *funcNotify)(cl_event, cl_int, void *),
                                      void *userData) {
    TRACING_ENTER(ClSetEventCallback, &event, &commandExecCallbackType, &funcNotify, &userData);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event), std::make_tuple(reinterpret_cast<void *>(funcNotify)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClSetEventCallback, &retVal);
        return retVal;
    }

    switch (commandExecCallbackType) {
    default: {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClSetEventCallback, &tracingRetVal);
        return tracingRetVal;
    }
    case CL_QUEUED:
    case CL_SUBMITTED: {
        funcNotify(event, commandExecCallbackType, userData);
        cl_int tracingRetVal = CL_SUCCESS;
        TRACING_EXIT(ClSetEventCallback, &tracingRetVal);
        return tracingRetVal;
    }
    case CL_RUNNING:
    case CL_COMPLETE: {
        if (pEvent->queryAndUpdateEventStatus() == CL_COMPLETE) {
            funcNotify(event, commandExecCallbackType, userData);
            cl_int tracingRetVal = CL_SUCCESS;
            TRACING_EXIT(ClSetEventCallback, &tracingRetVal);
            return tracingRetVal;
        } else {
            NEO::LEO::EventHandleSpan waitEvents{1, &event};
            auto clUserData = new NEO::LEO::Event::ClUserData{event, commandExecCallbackType, funcNotify, userData};

            pEvent->incRefInternal();

            ze_command_list_handle_t cmdList;
            std::unique_lock<std::mutex> internalLock;
            std::unique_lock<std::recursive_mutex> cmdQLock;
            if (pEvent->isUserEvent()) {
                internalLock = pEvent->getContext()->lockInternalCompute();
                cmdList = pEvent->getContext()->getInternalComputeCmdList();
            } else {
                cmdQLock = pEvent->getCommandQueue()->takeOwnership();
                cmdList = pEvent->getCommandQueue()->getL0Handle();
            }

            cl_int tracingRetVal = L0ToClResultMapper(L0::zeCommandListAppendHostFunction(cmdList,
                                                                                          reinterpret_cast<ze_host_function_callback_t>(NEO::LEO::Event::clCallbackWrapper),
                                                                                          clUserData,
                                                                                          nullptr,
                                                                                          nullptr,
                                                                                          waitEvents.size(),
                                                                                          waitEvents.data()));
            TRACING_EXIT(ClSetEventCallback, &tracingRetVal);
            return tracingRetVal;
        }
    }
    }
}

cl_int CL_API_CALL clGetEventProfilingInfo(cl_event event,
                                           cl_profiling_info paramName,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetEventProfilingInfo, &event, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetEventProfilingInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pEvent->getProfilingInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetEventProfilingInfo, &tracingRetVal);
    return tracingRetVal;
}
