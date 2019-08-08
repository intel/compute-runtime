/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/tracing/tracing_api.h"

#include "runtime/tracing/tracing_handle.h"
#include "runtime/tracing/tracing_notify.h"

namespace HostSideTracing {

// [XYZZ..Z] - { X - enabled/disabled bit, Y - locked/unlocked bit, ZZ..Z - client count bits }
std::atomic<uint32_t> tracingState(0);
std::vector<TracingHandle *> tracingHandle;
std::atomic<uint32_t> tracingCorrelationId(0);

bool addTracingClient() {
    uint32_t state = tracingState.load(std::memory_order_acquire);
    state = TRACING_SET_ENABLED_BIT(state);
    state = TRACING_UNSET_LOCKED_BIT(state);
    AtomicBackoff backoff;
    while (!tracingState.compare_exchange_weak(state, state + 1, std::memory_order_release,
                                               std::memory_order_acquire)) {
        if (!TRACING_GET_ENABLED_BIT(state)) {
            return false;
        } else if (TRACING_GET_LOCKED_BIT(state)) {
            DEBUG_BREAK_IF(TRACING_GET_CLIENT_COUNTER(state) != 0);
            state = TRACING_UNSET_LOCKED_BIT(state);
            backoff.pause();
        } else {
            backoff.pause();
        }
    }
    return true;
}

void removeTracingClient() {
    DEBUG_BREAK_IF(!TRACING_GET_ENABLED_BIT(tracingState.load(std::memory_order_acquire)));
    DEBUG_BREAK_IF(TRACING_GET_LOCKED_BIT(tracingState.load(std::memory_order_acquire)));
    DEBUG_BREAK_IF(TRACING_GET_CLIENT_COUNTER(tracingState.load(std::memory_order_acquire)) == 0);
    tracingState.fetch_sub(1, std::memory_order_acq_rel);
}

static void LockTracingState() {
    uint32_t state = tracingState.load(std::memory_order_acquire);
    state = TRACING_ZERO_CLIENT_COUNTER(state);
    state = TRACING_UNSET_LOCKED_BIT(state);
    AtomicBackoff backoff;
    while (!tracingState.compare_exchange_weak(state, TRACING_SET_LOCKED_BIT(state),
                                               std::memory_order_release, std::memory_order_acquire)) {
        state = TRACING_ZERO_CLIENT_COUNTER(state);
        state = TRACING_UNSET_LOCKED_BIT(state);
        backoff.pause();
    }
    DEBUG_BREAK_IF(!TRACING_GET_LOCKED_BIT(tracingState.load(std::memory_order_acquire)));
    DEBUG_BREAK_IF(TRACING_GET_CLIENT_COUNTER(tracingState.load(std::memory_order_acquire)) > 0);
}

static void UnlockTracingState() {
    DEBUG_BREAK_IF(!TRACING_GET_LOCKED_BIT(tracingState.load(std::memory_order_acquire)));
    DEBUG_BREAK_IF(TRACING_GET_CLIENT_COUNTER(tracingState.load(std::memory_order_acquire)) > 0);
    tracingState.fetch_and(~TRACING_STATE_LOCKED_BIT, std::memory_order_acq_rel);
}

} // namespace HostSideTracing

using namespace HostSideTracing;

cl_int CL_API_CALL clCreateTracingHandleINTEL(cl_device_id device, cl_tracing_callback callback, void *userData, cl_tracing_handle *handle) {
    if (device == nullptr || callback == nullptr || handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    *handle = new _cl_tracing_handle;
    if (*handle == nullptr) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    (*handle)->device = device;
    (*handle)->handle = new TracingHandle(callback, userData);
    if ((*handle)->handle == nullptr) {
        delete *handle;
        return CL_OUT_OF_HOST_MEMORY;
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clSetTracingPointINTEL(cl_tracing_handle handle, cl_function_id fid, cl_bool enable) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    DEBUG_BREAK_IF(handle->handle == nullptr);
    if (static_cast<uint32_t>(fid) >= CL_FUNCTION_COUNT) {
        return CL_INVALID_VALUE;
    }

    handle->handle->setTracingPoint(fid, enable);

    return CL_SUCCESS;
}

cl_int CL_API_CALL clDestroyTracingHandleINTEL(cl_tracing_handle handle) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    DEBUG_BREAK_IF(handle->handle == nullptr);
    delete handle->handle;
    delete handle;

    return CL_SUCCESS;
}

cl_int CL_API_CALL clEnableTracingINTEL(cl_tracing_handle handle) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    LockTracingState();

    DEBUG_BREAK_IF(handle->handle == nullptr);
    for (size_t i = 0; i < tracingHandle.size(); ++i) {
        if (tracingHandle[i] == handle->handle) {
            UnlockTracingState();
            return CL_INVALID_VALUE;
        }
    }

    if (tracingHandle.size() == TRACING_MAX_HANDLE_COUNT) {
        UnlockTracingState();
        return CL_OUT_OF_RESOURCES;
    }

    tracingHandle.push_back(handle->handle);
    if (tracingHandle.size() == 1) {
        tracingState.fetch_or(TRACING_STATE_ENABLED_BIT, std::memory_order_acq_rel);
    }

    UnlockTracingState();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clDisableTracingINTEL(cl_tracing_handle handle) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    LockTracingState();

    DEBUG_BREAK_IF(handle->handle == nullptr);
    for (size_t i = 0; i < tracingHandle.size(); ++i) {
        if (tracingHandle[i] == handle->handle) {
            if (tracingHandle.size() == 1) {
                tracingState.fetch_and(~TRACING_STATE_ENABLED_BIT, std::memory_order_acq_rel);
                std::vector<TracingHandle *>().swap(tracingHandle);
            } else {
                tracingHandle[i] = tracingHandle[tracingHandle.size() - 1];
                tracingHandle.pop_back();
            }
            UnlockTracingState();
            return CL_SUCCESS;
        }
    }

    UnlockTracingState();
    return CL_INVALID_VALUE;
}

cl_int CL_API_CALL clGetTracingStateINTEL(cl_tracing_handle handle, cl_bool *enable) {
    if (handle == nullptr || enable == nullptr) {
        return CL_INVALID_VALUE;
    }

    LockTracingState();

    *enable = CL_FALSE;

    DEBUG_BREAK_IF(handle->handle == nullptr);
    for (size_t i = 0; i < tracingHandle.size(); ++i) {
        if (tracingHandle[i] == handle->handle) {
            *enable = CL_TRUE;
            break;
        }
    }

    UnlockTracingState();
    return CL_SUCCESS;
}
