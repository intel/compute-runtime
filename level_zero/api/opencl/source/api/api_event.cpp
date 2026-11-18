/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_int CL_API_CALL clWaitForEvents(cl_uint numEvents,
                                   const cl_event *eventList) {
    TRACING_ENTER(ClWaitForEvents, &numEvents, &eventList);
    auto [retVal] = NEO::LEO::validateAndCast(std::make_tuple(), std::make_tuple(NEO::LEO::EventWaitList{eventList, numEvents}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    for (cl_uint i = 0; i < numEvents; ++i) {
        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(eventList[i]);
        if (pEvent->getContext()->isTerminated()) {
            return tracingScope.exit(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
        }

        auto ret = pEvent->wait();
        if (ret != ZE_RESULT_SUCCESS) {
            return tracingScope.exit(L0ToClResultMapper(ret));
        }
        pEvent->updateExecutionStatus();
    }
    return tracingScope.exit(CL_SUCCESS);
}

cl_int CL_API_CALL clGetEventInfo(cl_event event,
                                  cl_event_info paramName,
                                  size_t paramValueSize,
                                  void *paramValue,
                                  size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetEventInfo, &event, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(pEvent->getEventInfo(paramName, paramValueSize, paramValue, paramValueSizeRet));
}

cl_event CL_API_CALL clCreateUserEvent(cl_context context,
                                       cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateUserEvent, &context, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return tracingScope.exit(nullptr);
    }
    return tracingScope.exit(new NEO::LEO::Event(pContext));
}

cl_int CL_API_CALL clRetainEvent(cl_event event) {
    TRACING_ENTER(ClRetainEvent, &event);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    pEvent->incRefApi();
    return tracingScope.exit(CL_SUCCESS);
}

cl_int CL_API_CALL clReleaseEvent(cl_event event) {
    TRACING_ENTER(ClReleaseEvent, &event);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    pEvent->decRefApi();
    return tracingScope.exit(CL_SUCCESS);
}

cl_int CL_API_CALL clSetUserEventStatus(cl_event event,
                                        cl_int executionStatus) {
    TRACING_ENTER(ClSetUserEventStatus, &event, &executionStatus);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    if (executionStatus != CL_COMPLETE) {
        pEvent->getContext()->terminateExecution();
    }
    return tracingScope.exit(L0ToClResultMapper(pEvent->signal(executionStatus)));
}

cl_int CL_API_CALL clSetEventCallback(cl_event event,
                                      cl_int commandExecCallbackType,
                                      void(CL_CALLBACK *funcNotify)(cl_event, cl_int, void *),
                                      void *userData) {
    TRACING_ENTER(ClSetEventCallback, &event, &commandExecCallbackType, &funcNotify, &userData);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event), std::make_tuple(reinterpret_cast<void *>(funcNotify)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    switch (commandExecCallbackType) {
    default: {
        return tracingScope.exit(CL_INVALID_VALUE);
    }
    case CL_QUEUED:
    case CL_SUBMITTED: {
        funcNotify(event, commandExecCallbackType, userData);
        return tracingScope.exit(CL_SUCCESS);
    }
    case CL_RUNNING:
    case CL_COMPLETE: {
        if (pEvent->queryAndUpdateEventStatus() == CL_COMPLETE) {
            if (commandExecCallbackType == CL_COMPLETE) {
                pEvent->makeResultsVisibleToHost();
            }
            funcNotify(event, commandExecCallbackType, userData);
        } else {
            pEvent->addCallback(funcNotify, commandExecCallbackType, userData);
        }
    }
    }
    return tracingScope.exit(CL_SUCCESS);
}

cl_int CL_API_CALL clGetEventProfilingInfo(cl_event event,
                                           cl_profiling_info paramName,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetEventProfilingInfo, &event, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    return tracingScope.exit(pEvent->getProfilingInfo(paramName, paramValueSize, paramValue, paramValueSizeRet));
}
