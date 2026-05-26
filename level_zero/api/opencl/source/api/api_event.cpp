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
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_int CL_API_CALL clWaitForEvents(cl_uint numEvents,
                                   const cl_event *eventList) {
    auto [retVal] = NEO::LEO::validateAndCast(std::make_tuple(), std::make_tuple(NEO::LEO::EventWaitList{eventList, numEvents}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    for (cl_uint i = 0; i < numEvents; ++i) {
        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(eventList[i]);
        if (pEvent->getContext()->isTerminated()) {
            return CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
        }

        auto ret = pEvent->wait();
        if (ret != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(ret);
        }
    }
    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetEventInfo(cl_event event,
                                  cl_event_info paramName,
                                  size_t paramValueSize,
                                  void *paramValue,
                                  size_t *paramValueSizeRet) {
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pEvent->getEventInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_event CL_API_CALL clCreateUserEvent(cl_context context,
                                       cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }
    return new NEO::LEO::Event(pContext);
}

cl_int CL_API_CALL clRetainEvent(cl_event event) {
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pEvent->incRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clReleaseEvent(cl_event event) {
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pEvent->decRefApi();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clSetUserEventStatus(cl_event event,
                                        cl_int executionStatus) {
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (executionStatus != CL_COMPLETE) {
        pEvent->getContext()->terminateExecution();
    }
    return pEvent->signal();
}

cl_int CL_API_CALL clSetEventCallback(cl_event event,
                                      cl_int commandExecCallbackType,
                                      void(CL_CALLBACK *funcNotify)(cl_event, cl_int, void *),
                                      void *userData) {
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event), std::make_tuple(reinterpret_cast<void *>(funcNotify)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    switch (commandExecCallbackType) {
    default:
        return CL_INVALID_VALUE;
    case CL_QUEUED:
    case CL_SUBMITTED:
        funcNotify(event, commandExecCallbackType, userData);
        return CL_SUCCESS;
    case CL_RUNNING:
    case CL_COMPLETE: {
        if (pEvent->queryAndUpdateEventStatus() == CL_COMPLETE) {
            funcNotify(event, commandExecCallbackType, userData);
            return CL_SUCCESS;
        } else {
            NEO::LEO::EventHandleSpan waitEvents{1, &event};
            auto clUserData = new NEO::LEO::Event::ClUserData{event, commandExecCallbackType, funcNotify, userData};

            pEvent->incRefInternal();

            ze_command_list_handle_t cmdList;
            std::unique_lock<std::mutex> internalLock;
            if (pEvent->isUserEvent()) {
                internalLock = pEvent->getContext()->lockInternalCompute();
                cmdList = pEvent->getContext()->getInternalComputeCmdList();
            } else {
                cmdList = pEvent->getCommandQueue()->getL0Handle();
            }

            return L0ToClResultMapper(L0::zeCommandListAppendHostFunction(cmdList,
                                                                          reinterpret_cast<ze_host_function_callback_t>(NEO::LEO::Event::clCallbackWrapper),
                                                                          clUserData,
                                                                          nullptr,
                                                                          nullptr,
                                                                          waitEvents.size(),
                                                                          waitEvents.data()));
        }
    }
    }
}

cl_int CL_API_CALL clGetEventProfilingInfo(cl_event event,
                                           cl_profiling_info paramName,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet) {
    auto [retVal, pEvent] = NEO::LEO::validateAndCast(std::make_tuple(event));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    return pEvent->getProfilingInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}
