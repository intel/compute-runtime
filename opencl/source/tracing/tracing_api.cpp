/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tracing/tracing_api.h"

#include "opencl/source/tracing/tracing_handle.h"
#include "opencl/source/tracing/tracing_notify.h"

namespace HostSideTracing {

// [XYZZ..Z] - { X - enabled/disabled bit, Y - locked/unlocked bit, ZZ..Z - client count bits }
std::atomic<uint32_t> tracingState(0);
TracingHandle *tracingHandle[TRACING_MAX_HANDLE_COUNT] = {nullptr};
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

static void lockTracingState() {
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

static void unlockTracingState() {
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

    lockTracingState();

    size_t i = 0;
    DEBUG_BREAK_IF(handle->handle == nullptr);
    while (i < TRACING_MAX_HANDLE_COUNT && tracingHandle[i] != nullptr) {
        if (tracingHandle[i] == handle->handle) {
            unlockTracingState();
            return CL_INVALID_VALUE;
        }
        ++i;
    }

    if (i == TRACING_MAX_HANDLE_COUNT) {
        unlockTracingState();
        return CL_OUT_OF_RESOURCES;
    }

    DEBUG_BREAK_IF(tracingHandle[i] != nullptr);
    tracingHandle[i] = handle->handle;
    if (i == 0) {
        tracingState.fetch_or(TRACING_STATE_ENABLED_BIT, std::memory_order_acq_rel);
    }

    unlockTracingState();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clDisableTracingINTEL(cl_tracing_handle handle) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    lockTracingState();

    size_t size = 0;
    while (size < TRACING_MAX_HANDLE_COUNT && tracingHandle[size] != nullptr) {
        ++size;
    }

    size_t i = 0;
    DEBUG_BREAK_IF(handle->handle == nullptr);
    while (i < TRACING_MAX_HANDLE_COUNT && tracingHandle[i] != nullptr) {
        if (tracingHandle[i] == handle->handle) {
            if (size == 1) {
                DEBUG_BREAK_IF(i != 0);
                tracingState.fetch_and(~TRACING_STATE_ENABLED_BIT, std::memory_order_acq_rel);
                tracingHandle[i] = nullptr;
            } else {
                tracingHandle[i] = tracingHandle[size - 1];
                tracingHandle[size - 1] = nullptr;
            }
            unlockTracingState();
            return CL_SUCCESS;
        }
        ++i;
    }

    unlockTracingState();
    return CL_INVALID_VALUE;
}

cl_int CL_API_CALL clGetTracingStateINTEL(cl_tracing_handle handle, cl_bool *enable) {
    if (handle == nullptr || enable == nullptr) {
        return CL_INVALID_VALUE;
    }

    lockTracingState();

    *enable = CL_FALSE;

    size_t i = 0;
    DEBUG_BREAK_IF(handle->handle == nullptr);
    while (i < TRACING_MAX_HANDLE_COUNT && tracingHandle[i] != nullptr) {
        if (tracingHandle[i] == handle->handle) {
            *enable = CL_TRUE;
            break;
        }
        ++i;
    }

    unlockTracingState();
    return CL_SUCCESS;
}
