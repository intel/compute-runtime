/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tracing/tracing_api.h"

#include "opencl/source/api/leo_forwarding.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/tracing/tracing_handle.h"
#include "opencl/source/tracing/tracing_notify.h"

#include <mutex>
#include <set>

namespace HostSideTracing {

namespace {
std::mutex ownedTracingHandlesMutex;
std::set<cl_tracing_handle> ownedTracingHandles;

bool isForeignTracingHandle(cl_tracing_handle handle) {
    if (!NEO::hasLeoPlatforms()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(ownedTracingHandlesMutex);
    return ownedTracingHandles.find(handle) == ownedTracingHandles.end();
}
} // namespace

#define FORWARD_TRACING_TO_LEO_IF_FOREIGN(handle, name, ...)                                                      \
    if (isForeignTracingHandle(handle)) {                                                                         \
        if (auto leoFunc = reinterpret_cast<decltype(&name)>(NEO::forwardClGetExtensionFunctionAddress(#name))) { \
            return leoFunc(__VA_ARGS__);                                                                          \
        }                                                                                                         \
    }

// [XYZZ..Z] - { X - enabled/disabled bit, Y - locked/unlocked bit, ZZ..Z - client count bits }
std::atomic<uint32_t> tracingState(0);
TracingHandle *tracingHandle[tracingMaxHandleCount] = {nullptr};
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
    tracingState.fetch_and(~tracingStateLockedBit, std::memory_order_acq_rel);
}

} // namespace HostSideTracing

using namespace HostSideTracing;

cl_int CL_API_CALL clCreateTracingHandleINTEL(cl_device_id device, cl_tracing_callback callback, void *userData, cl_tracing_handle *handle) {
    if (device == nullptr || callback == nullptr || handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    if (nullptr == NEO::castToObject<NEO::ClDevice>(device) && NEO::hasLeoPlatforms()) {
        if (auto leoFunc = reinterpret_cast<decltype(&clCreateTracingHandleINTEL)>(NEO::forwardClGetExtensionFunctionAddress("clCreateTracingHandleINTEL"))) {
            return leoFunc(device, callback, userData, handle);
        }
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

    {
        std::lock_guard<std::mutex> lock(ownedTracingHandlesMutex);
        ownedTracingHandles.insert(*handle);
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clSetTracingPointINTEL(cl_tracing_handle handle, ClFunctionId fid, cl_bool enable) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    FORWARD_TRACING_TO_LEO_IF_FOREIGN(handle, clSetTracingPointINTEL, handle, fid, enable);

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

    FORWARD_TRACING_TO_LEO_IF_FOREIGN(handle, clDestroyTracingHandleINTEL, handle);

    DEBUG_BREAK_IF(handle->handle == nullptr);
    {
        std::lock_guard<std::mutex> lock(ownedTracingHandlesMutex);
        ownedTracingHandles.erase(handle);
    }
    delete handle->handle;
    delete handle;

    return CL_SUCCESS;
}

cl_int CL_API_CALL clEnableTracingINTEL(cl_tracing_handle handle) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    FORWARD_TRACING_TO_LEO_IF_FOREIGN(handle, clEnableTracingINTEL, handle);

    lockTracingState();

    size_t i = 0;
    DEBUG_BREAK_IF(handle->handle == nullptr);
    while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
        if (tracingHandle[i] == handle->handle) {
            unlockTracingState();
            return CL_INVALID_VALUE;
        }
        ++i;
    }

    if (i == tracingMaxHandleCount) {
        unlockTracingState();
        return CL_OUT_OF_RESOURCES;
    }

    DEBUG_BREAK_IF(tracingHandle[i] != nullptr);
    tracingHandle[i] = handle->handle;
    if (i == 0) {
        tracingState.fetch_or(tracingStateEnabledBit, std::memory_order_acq_rel);
    }

    unlockTracingState();
    return CL_SUCCESS;
}

cl_int CL_API_CALL clDisableTracingINTEL(cl_tracing_handle handle) {
    if (handle == nullptr) {
        return CL_INVALID_VALUE;
    }

    FORWARD_TRACING_TO_LEO_IF_FOREIGN(handle, clDisableTracingINTEL, handle);

    lockTracingState();

    size_t size = 0;
    while (size < tracingMaxHandleCount && tracingHandle[size] != nullptr) {
        ++size;
    }

    size_t i = 0;
    DEBUG_BREAK_IF(handle->handle == nullptr);
    while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
        if (tracingHandle[i] == handle->handle) {
            if (size == 1) {
                DEBUG_BREAK_IF(i != 0);
                tracingState.fetch_and(~tracingStateEnabledBit, std::memory_order_acq_rel);
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

    FORWARD_TRACING_TO_LEO_IF_FOREIGN(handle, clGetTracingStateINTEL, handle, enable);

    lockTracingState();

    *enable = CL_FALSE;

    size_t i = 0;
    DEBUG_BREAK_IF(handle->handle == nullptr);
    while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
        if (tracingHandle[i] == handle->handle) {
            *enable = CL_TRUE;
            break;
        }
        ++i;
    }

    unlockTracingState();
    return CL_SUCCESS;
}
