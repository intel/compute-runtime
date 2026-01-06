/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "opencl/source/tracing/tracing_handle.h"

#include <atomic>
#include <thread>

namespace HostSideTracing {

#define TRACING_SET_ENABLED_BIT(state) ((state) | (HostSideTracing::tracingStateEnabledBit))
#define TRACING_UNSET_ENABLED_BIT(state) ((state) & (~HostSideTracing::tracingStateEnabledBit))
#define TRACING_GET_ENABLED_BIT(state) ((state) & (HostSideTracing::tracingStateEnabledBit))

#define TRACING_SET_LOCKED_BIT(state) ((state) | (HostSideTracing::tracingStateLockedBit))
#define TRACING_UNSET_LOCKED_BIT(state) ((state) & (~HostSideTracing::tracingStateLockedBit))
#define TRACING_GET_LOCKED_BIT(state) ((state) & (HostSideTracing::tracingStateLockedBit))

#define TRACING_ZERO_CLIENT_COUNTER(state) ((state) & (HostSideTracing::tracingStateEnabledBit | HostSideTracing::tracingStateLockedBit))
#define TRACING_GET_CLIENT_COUNTER(state) ((state) & (~(HostSideTracing::tracingStateEnabledBit | HostSideTracing::tracingStateLockedBit)))

inline thread_local bool tracingInProgress = false;

class CheckIfExitCalled : NEO::NonCopyableAndNonMovableClass {
  public:
    CheckIfExitCalled() = default;
    ~CheckIfExitCalled() {
        UNRECOVERABLE_IF(!tracingExited);
    }

    void exit() {
        tracingExited = true;
    }

  private:
    bool tracingExited = false;
};

static_assert(NEO::NonCopyableAndNonMovable<CheckIfExitCalled>);

#define TRACING_ENTER(name, ...)                                                                                                                   \
    bool isHostSideTracingEnabled_##name = false;                                                                                                  \
    bool currentlyTracedCall = false;                                                                                                              \
    HostSideTracing::CheckIfExitCalled checkIfExited;                                                                                              \
    HostSideTracing::name##Tracer tracer_##name;                                                                                                   \
    if (TRACING_GET_ENABLED_BIT(HostSideTracing::tracingState.load(std::memory_order_acquire)) && (false == HostSideTracing::tracingInProgress)) { \
        HostSideTracing::tracingInProgress = true;                                                                                                 \
        currentlyTracedCall = true;                                                                                                                \
        isHostSideTracingEnabled_##name = HostSideTracing::addTracingClient();                                                                     \
        if (isHostSideTracingEnabled_##name) {                                                                                                     \
            tracer_##name.enter(__VA_ARGS__);                                                                                                      \
        }                                                                                                                                          \
    }

#define TRACING_EXIT(name, ...)                     \
    if (currentlyTracedCall) {                      \
        if (isHostSideTracingEnabled_##name) {      \
            tracer_##name.exit(__VA_ARGS__);        \
            HostSideTracing::removeTracingClient(); \
        }                                           \
        HostSideTracing::tracingInProgress = false; \
        currentlyTracedCall = false;                \
    }                                               \
    checkIfExited.exit();

enum TracingNotifyState {
    TRACING_NOTIFY_STATE_NOTHING_CALLED = 0,
    TRACING_NOTIFY_STATE_ENTER_CALLED = 1,
    TRACING_NOTIFY_STATE_EXIT_CALLED = 2,
};

inline constexpr size_t tracingMaxHandleCount = 16;

inline constexpr uint32_t tracingStateEnabledBit = 0x80000000u;
inline constexpr uint32_t tracingStateLockedBit = 0x40000000u;

extern std::atomic<uint32_t> tracingState;
extern TracingHandle *tracingHandle[tracingMaxHandleCount];
extern std::atomic<uint32_t> tracingCorrelationId;

bool addTracingClient();
void removeTracingClient();

class AtomicBackoff {
  public:
    AtomicBackoff() {}

    void pause() {
        if (count < loopsBeforeYield) {
            for (uint32_t i = 0; i < count; i++) {
                NEO::CpuIntrinsics::pause();
            }
            count *= 2;
        } else {
            std::this_thread::yield();
        }
    }

  private:
    static const uint32_t loopsBeforeYield = 16;
    uint32_t count = 1;
};

class ClBuildProgramTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClBuildProgramTracer() {}

    void enter(cl_program *program,
               cl_uint *numDevices,
               const cl_device_id **deviceList,
               const char **options,
               void(CL_CALLBACK **funcNotify)(cl_program program, void *userData),
               void **userData) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.numDevices = numDevices;
        params.deviceList = deviceList;
        params.options = options;
        params.funcNotify = funcNotify;
        params.userData = userData;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clBuildProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clBuildProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clBuildProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clBuildProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clBuildProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClBuildProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clBuildProgram params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCloneKernelTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCloneKernelTracer() {}

    void enter(cl_kernel *sourceKernel,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.sourceKernel = sourceKernel;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCloneKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCloneKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCloneKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_kernel *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCloneKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCloneKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCloneKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCloneKernel params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCompileProgramTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCompileProgramTracer() {}

    void enter(cl_program *program,
               cl_uint *numDevices,
               const cl_device_id **deviceList,
               const char **options,
               cl_uint *numInputHeaders,
               const cl_program **inputHeaders,
               const char ***headerIncludeNames,
               void(CL_CALLBACK **funcNotify)(cl_program program, void *userData),
               void **userData) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.numDevices = numDevices;
        params.deviceList = deviceList;
        params.options = options;
        params.numInputHeaders = numInputHeaders;
        params.inputHeaders = inputHeaders;
        params.headerIncludeNames = headerIncludeNames;
        params.funcNotify = funcNotify;
        params.userData = userData;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCompileProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCompileProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCompileProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCompileProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCompileProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCompileProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCompileProgram params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateBufferTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               size_t *size,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.size = size;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateCommandQueueTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateCommandQueueTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               const cl_command_queue_properties *properties,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.device = device;
        params.properties = properties;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateCommandQueue";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_command_queue *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateCommandQueue params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateCommandQueueWithPropertiesTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateCommandQueueWithPropertiesTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               const cl_queue_properties **properties,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.device = device;
        params.properties = properties;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateCommandQueueWithProperties";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueueWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueueWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_command_queue *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueueWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueueWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateCommandQueueWithPropertiesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateCommandQueueWithProperties params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateContextTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateContextTracer() {}

    void enter(const cl_context_properties **properties,
               cl_uint *numDevices,
               const cl_device_id **devices,
               void(CL_CALLBACK **funcNotify)(const char *, const void *, size_t, void *),
               void **userData,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.properties = properties;
        params.numDevices = numDevices;
        params.devices = devices;
        params.funcNotify = funcNotify;
        params.userData = userData;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateContext";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContext, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_context *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContext, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateContextTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateContext params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateContextFromTypeTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateContextFromTypeTracer() {}

    void enter(const cl_context_properties **properties,
               cl_device_type *deviceType,
               void(CL_CALLBACK **funcNotify)(const char *, const void *, size_t, void *),
               void **userData,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.properties = properties;
        params.deviceType = deviceType;
        params.funcNotify = funcNotify;
        params.userData = userData;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateContextFromType";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContextFromType)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContextFromType, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_context *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContextFromType)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContextFromType, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateContextFromTypeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateContextFromType params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClMemFreeINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClMemFreeINTELTracer() {}

    void enter(cl_context *context,
               void **ptr) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.ptr = ptr;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clMemFreeINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clMemFreeINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clMemFreeINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clMemFreeINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clMemFreeINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClMemFreeINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clMemFreeINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClIcdGetPlatformIDsKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClIcdGetPlatformIDsKHRTracer() {}

    void enter(cl_uint *numEntries,
               cl_platform_id **platforms,
               cl_uint **numPlatforms) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.numEntries = numEntries;
        params.platforms = platforms;
        params.numPlatforms = numPlatforms;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clIcdGetPlatformIDsKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clIcdGetPlatformIDsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clIcdGetPlatformIDsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clIcdGetPlatformIDsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clIcdGetPlatformIDsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClIcdGetPlatformIDsKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clIcdGetPlatformIDsKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateBufferWithPropertiesINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateBufferWithPropertiesINTELTracer() {}

    void enter(cl_context *context,
               const cl_mem_properties **properties,
               cl_mem_flags *flags,
               size_t *size,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.properties = properties;
        params.flags = flags;
        params.size = size;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateBufferWithPropertiesINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBufferWithPropertiesINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBufferWithPropertiesINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBufferWithPropertiesINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBufferWithPropertiesINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateBufferWithPropertiesINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateBufferWithPropertiesINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateBufferWithPropertiesTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateBufferWithPropertiesTracer() {}

    void enter(cl_context *context,
               const cl_mem_properties **properties,
               cl_mem_flags *flags,
               size_t *size,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.properties = properties;
        params.flags = flags;
        params.size = size;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateBufferWithProperties";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBufferWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBufferWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBufferWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBufferWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateBufferWithPropertiesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateBufferWithProperties params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateImageWithPropertiesTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateImageWithPropertiesTracer() {}

    void enter(cl_context *context,
               const cl_mem_properties_intel **properties,
               cl_mem_flags *flags,
               const cl_image_format **imageFormat,
               const cl_image_desc **imageDesc,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.properties = properties;
        params.flags = flags;
        params.imageFormat = imageFormat;
        params.imageDesc = imageDesc;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateImageWithProperties";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImageWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImageWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImageWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImageWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateImageWithPropertiesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImageWithProperties params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateImageWithPropertiesINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateImageWithPropertiesINTELTracer() {}

    void enter(cl_context *context,
               const cl_mem_properties_intel **properties,
               cl_mem_flags *flags,
               const cl_image_format **imageFormat,
               const cl_image_desc **imageDesc,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.properties = properties;
        params.flags = flags;
        params.imageFormat = imageFormat;
        params.imageDesc = imageDesc;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateImageWithPropertiesINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImageWithPropertiesINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImageWithPropertiesINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImageWithPropertiesINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImageWithPropertiesINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateImageWithPropertiesINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImageWithPropertiesINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetImageParamsINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetImageParamsINTELTracer() {}

    void enter(cl_context *context,
               const cl_image_format **imageFormat,
               const cl_image_desc **imageDesc,
               size_t **imageRowPitch,
               size_t **imageSlicePitch) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.imageFormat = imageFormat;
        params.imageDesc = imageDesc;
        params.imageRowPitch = imageRowPitch;
        params.imageSlicePitch = imageSlicePitch;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetImageParamsINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetImageParamsINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetImageParamsINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetImageParamsINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetImageParamsINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetImageParamsINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetImageParamsINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreatePerfCountersCommandQueueINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreatePerfCountersCommandQueueINTELTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               cl_command_queue_properties *properties,
               cl_uint *configuration,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.device = device;
        params.properties = properties;
        params.configuration = configuration;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreatePerfCountersCommandQueueINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreatePerfCountersCommandQueueINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreatePerfCountersCommandQueueINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_command_queue *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreatePerfCountersCommandQueueINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreatePerfCountersCommandQueueINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreatePerfCountersCommandQueueINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreatePerfCountersCommandQueueINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClHostMemAllocINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClHostMemAllocINTELTracer() {}

    void enter(cl_context *context,
               const cl_mem_properties_intel **properties,
               size_t *size,
               cl_uint *alignment,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.properties = properties;
        params.size = size;
        params.alignment = alignment;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clHostMemAllocINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clHostMemAllocINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clHostMemAllocINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clHostMemAllocINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clHostMemAllocINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClHostMemAllocINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clHostMemAllocINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClDeviceMemAllocINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClDeviceMemAllocINTELTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               const cl_mem_properties_intel **properties,
               size_t *size,
               cl_uint *alignment,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.device = device;
        params.properties = properties;
        params.size = size;
        params.alignment = alignment;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clDeviceMemAllocINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clDeviceMemAllocINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clDeviceMemAllocINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clDeviceMemAllocINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clDeviceMemAllocINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClDeviceMemAllocINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clDeviceMemAllocINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSharedMemAllocINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSharedMemAllocINTELTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               const cl_mem_properties_intel **properties,
               size_t *size,
               cl_uint *alignment,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.device = device;
        params.properties = properties;
        params.size = size;
        params.alignment = alignment;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSharedMemAllocINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSharedMemAllocINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSharedMemAllocINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSharedMemAllocINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSharedMemAllocINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSharedMemAllocINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSharedMemAllocINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClMemBlockingFreeINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClMemBlockingFreeINTELTracer() {}

    void enter(cl_context *context,
               void **ptr) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.ptr = ptr;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clMemBlockingFreeINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clMemBlockingFreeINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clMemBlockingFreeINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clMemBlockingFreeINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clMemBlockingFreeINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClMemBlockingFreeINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clMemBlockingFreeINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetMemAllocInfoINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetMemAllocInfoINTELTracer() {}

    void enter(cl_context *context,
               const void **ptr,
               cl_mem_info_intel *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.ptr = ptr;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetMemAllocInfoINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetMemAllocInfoINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetMemAllocInfoINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetMemAllocInfoINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetMemAllocInfoINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetMemAllocInfoINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetMemAllocInfoINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetKernelArgMemPointerINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetKernelArgMemPointerINTELTracer() {}

    void enter(cl_kernel *kernel,
               cl_uint *argIndex,
               const void **argValue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.argIndex = argIndex;
        params.argValue = argValue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetKernelArgMemPointerINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArgMemPointerINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArgMemPointerINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArgMemPointerINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArgMemPointerINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetKernelArgMemPointerINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetKernelArgMemPointerINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMemsetINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMemsetINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               void **dstPtr,
               cl_int *value,
               size_t *size,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.dstPtr = dstPtr;
        params.value = value;
        params.size = size;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMemsetINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemsetINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemsetINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemsetINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemsetINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMemsetINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMemsetINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMemFillINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMemFillINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               void **dstPtr,
               const void **pattern,
               size_t *patternSize,
               size_t *size,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.dstPtr = dstPtr;
        params.pattern = pattern;
        params.patternSize = patternSize;
        params.size = size;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMemFillINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemFillINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemFillINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemFillINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemFillINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMemFillINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMemFillINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMemcpyINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMemcpyINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_bool *blocking,
               void **dstPtr,
               const void **srcPtr,
               size_t *size,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.blocking = blocking;
        params.dstPtr = dstPtr;
        params.srcPtr = srcPtr;
        params.size = size;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMemcpyINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemcpyINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemcpyINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemcpyINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemcpyINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMemcpyINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMemcpyINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMigrateMemINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMigrateMemINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               const void **ptr,
               size_t *size,
               cl_mem_migration_flags *flags,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.ptr = ptr;
        params.size = size;
        params.flags = flags;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMigrateMemINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMigrateMemINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMigrateMemINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMigrateMemINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMigrateMemINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMigrateMemINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMigrateMemINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMemAdviseINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMemAdviseINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               const void **ptr,
               size_t *size,
               cl_mem_advice_intel *advice,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.ptr = ptr;
        params.size = size;
        params.advice = advice;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMemAdviseINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemAdviseINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemAdviseINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMemAdviseINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMemAdviseINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMemAdviseINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMemAdviseINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateCommandQueueWithPropertiesKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateCommandQueueWithPropertiesKHRTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               const cl_queue_properties_khr **properties,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.device = device;
        params.properties = properties;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateCommandQueueWithPropertiesKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueueWithPropertiesKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueueWithPropertiesKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_command_queue *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueueWithPropertiesKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueueWithPropertiesKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateCommandQueueWithPropertiesKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateCommandQueueWithPropertiesKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateAcceleratorINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateAcceleratorINTELTracer() {}

    void enter(cl_context *context,
               cl_accelerator_type_intel *acceleratorType,
               size_t *descriptorSize,
               const void **descriptor,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.acceleratorType = acceleratorType;
        params.descriptorSize = descriptorSize;
        params.descriptor = descriptor;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateAcceleratorINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateAcceleratorINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateAcceleratorINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_accelerator_intel *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateAcceleratorINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateAcceleratorINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateAcceleratorINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateAcceleratorINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainAcceleratorINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainAcceleratorINTELTracer() {}

    void enter(cl_accelerator_intel *accelerator) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.accelerator = accelerator;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainAcceleratorINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainAcceleratorINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainAcceleratorINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainAcceleratorINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainAcceleratorINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainAcceleratorINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainAcceleratorINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetAcceleratorInfoINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetAcceleratorInfoINTELTracer() {}

    void enter(cl_accelerator_intel *accelerator,
               cl_accelerator_info_intel *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.accelerator = accelerator;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetAcceleratorInfoINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetAcceleratorInfoINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetAcceleratorInfoINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetAcceleratorInfoINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetAcceleratorInfoINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetAcceleratorInfoINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetAcceleratorInfoINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseAcceleratorINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseAcceleratorINTELTracer() {}

    void enter(cl_accelerator_intel *accelerator) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.accelerator = accelerator;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseAcceleratorINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseAcceleratorINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseAcceleratorINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseAcceleratorINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseAcceleratorINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseAcceleratorINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseAcceleratorINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateProgramWithILKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateProgramWithILKHRTracer() {}

    void enter(cl_context *context,
               const void **il,
               size_t *length,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.il = il;
        params.length = length;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateProgramWithILKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithILKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithILKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithILKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithILKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateProgramWithILKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithILKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelSuggestedLocalWorkSizeKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelSuggestedLocalWorkSizeKHRTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_kernel *kernel,
               cl_uint *workDim,
               const size_t **globalWorkOffset,
               const size_t **globalWorkSize,
               size_t **suggestedLocalWorkSize) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.kernel = kernel;
        params.workDim = workDim;
        params.globalWorkOffset = globalWorkOffset;
        params.globalWorkSize = globalWorkSize;
        params.suggestedLocalWorkSize = suggestedLocalWorkSize;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelSuggestedLocalWorkSizeKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelSuggestedLocalWorkSizeKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelSuggestedLocalWorkSizeKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelSubGroupInfoKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelSubGroupInfoKHRTracer() {}

    void enter(cl_kernel *kernel,
               cl_device_id *device,
               cl_kernel_sub_group_info *paramName,
               size_t *inputValueSize,
               const void **inputValue,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.device = device;
        params.paramName = paramName;
        params.inputValueSize = inputValueSize;
        params.inputValue = inputValue;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelSubGroupInfoKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSubGroupInfoKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSubGroupInfoKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSubGroupInfoKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSubGroupInfoKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelSubGroupInfoKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelSubGroupInfoKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueVerifyMemoryINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueVerifyMemoryINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               const void **allocationPtr,
               const void **expectedData,
               size_t *sizeOfComparison,
               cl_uint *comparisonMode) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.allocationPtr = allocationPtr;
        params.expectedData = expectedData;
        params.sizeOfComparison = sizeOfComparison;
        params.comparisonMode = comparisonMode;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueVerifyMemoryINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueVerifyMemoryINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueVerifyMemoryINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueVerifyMemoryINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueVerifyMemoryINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueVerifyMemoryINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueVerifyMemoryINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClAddCommentINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClAddCommentINTELTracer() {}

    void enter(cl_device_id *device,
               const char **comment) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;
        params.comment = comment;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clAddCommentINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clAddCommentINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clAddCommentINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clAddCommentINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clAddCommentINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClAddCommentINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clAddCommentINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetDeviceGlobalVariablePointerINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetDeviceGlobalVariablePointerINTELTracer() {}

    void enter(cl_device_id *device,
               cl_program *program,
               const char **globalVariableName,
               size_t **globalVariableSizeRet,
               void ***globalVariablePointerRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;
        params.program = program;
        params.globalVariableName = globalVariableName;
        params.globalVariableSizeRet = globalVariableSizeRet;
        params.globalVariablePointerRet = globalVariablePointerRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetDeviceGlobalVariablePointerINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceGlobalVariablePointerINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceGlobalVariablePointerINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceGlobalVariablePointerINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceGlobalVariablePointerINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetDeviceGlobalVariablePointerINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceGlobalVariablePointerINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetDeviceFunctionPointerINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetDeviceFunctionPointerINTELTracer() {}

    void enter(cl_device_id *device,
               cl_program *program,
               const char **functionName,
               cl_ulong **functionPointerRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;
        params.program = program;
        params.functionName = functionName;
        params.functionPointerRet = functionPointerRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetDeviceFunctionPointerINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceFunctionPointerINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceFunctionPointerINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceFunctionPointerINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceFunctionPointerINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetDeviceFunctionPointerINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceFunctionPointerINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetProgramReleaseCallbackTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetProgramReleaseCallbackTracer() {}

    void enter(cl_program *program,
               void(CL_CALLBACK **pfnNotify)(cl_program, void *),
               void **userData) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.pfnNotify = pfnNotify;
        params.userData = userData;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetProgramReleaseCallback";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetProgramReleaseCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetProgramReleaseCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetProgramReleaseCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetProgramReleaseCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetProgramReleaseCallbackTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetProgramReleaseCallback params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetProgramSpecializationConstantTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetProgramSpecializationConstantTracer() {}

    void enter(cl_program *program,
               cl_uint *specId,
               size_t *specSize,
               const void **specValue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.specId = specId;
        params.specSize = specSize;
        params.specValue = specValue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetProgramSpecializationConstant";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetProgramSpecializationConstant)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetProgramSpecializationConstant, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetProgramSpecializationConstant)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetProgramSpecializationConstant, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetProgramSpecializationConstantTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetProgramSpecializationConstant params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelSuggestedLocalWorkSizeINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelSuggestedLocalWorkSizeINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_kernel *kernel,
               cl_uint *workDim,
               const size_t **globalWorkOffset,
               const size_t **globalWorkSize,
               size_t **suggestedLocalWorkSize) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.kernel = kernel;
        params.workDim = workDim;
        params.globalWorkOffset = globalWorkOffset;
        params.globalWorkSize = globalWorkSize;
        params.suggestedLocalWorkSize = suggestedLocalWorkSize;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelSuggestedLocalWorkSizeINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSuggestedLocalWorkSizeINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelSuggestedLocalWorkSizeINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelSuggestedLocalWorkSizeINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelMaxConcurrentWorkGroupCountINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelMaxConcurrentWorkGroupCountINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_kernel *kernel,
               cl_uint *workDim,
               const size_t **globalWorkOffset,
               const size_t **localWorkSize,
               size_t **suggestedWorkGroupCount) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.kernel = kernel;
        params.workDim = workDim;
        params.globalWorkOffset = globalWorkOffset;
        params.localWorkSize = localWorkSize;
        params.suggestedWorkGroupCount = suggestedWorkGroupCount;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelMaxConcurrentWorkGroupCountINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelMaxConcurrentWorkGroupCountINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelMaxConcurrentWorkGroupCountINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelMaxConcurrentWorkGroupCountINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelMaxConcurrentWorkGroupCountINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelMaxConcurrentWorkGroupCountINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelMaxConcurrentWorkGroupCountINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueNDCountKernelINTELTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueNDCountKernelINTELTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_kernel *kernel,
               cl_uint *workDim,
               const size_t **globalWorkOffset,
               const size_t **workgroupCount,
               const size_t **localWorkSize,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.kernel = kernel;
        params.workDim = workDim;
        params.globalWorkOffset = globalWorkOffset;
        params.workgroupCount = workgroupCount;
        params.localWorkSize = localWorkSize;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueNDCountKernelINTEL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNDCountKernelINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNDCountKernelINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNDCountKernelINTEL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNDCountKernelINTEL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueNDCountKernelINTELTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueNDCountKernelINTEL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetContextDestructorCallbackTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetContextDestructorCallbackTracer() {}

    void enter(cl_context *context,
               void(CL_CALLBACK **pfnNotify)(cl_context, void *),
               void **userData) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.pfnNotify = pfnNotify;
        params.userData = userData;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetContextDestructorCallback";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetContextDestructorCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetContextDestructorCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetContextDestructorCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetContextDestructorCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetContextDestructorCallbackTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetContextDestructorCallback params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueExternalMemObjectsKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueExternalMemObjectsKHRTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numMemObjects,
               const cl_mem **memObjects,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numMemObjects = numMemObjects;
        params.memObjects = memObjects;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueExternalMemObjectsKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueExternalMemObjectsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueExternalMemObjectsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueExternalMemObjectsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueExternalMemObjectsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueExternalMemObjectsKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueExternalMemObjectsKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueAcquireExternalMemObjectsKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueAcquireExternalMemObjectsKHRTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numMemObjects,
               const cl_mem **memObjects,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numMemObjects = numMemObjects;
        params.memObjects = memObjects;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueAcquireExternalMemObjectsKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueAcquireExternalMemObjectsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueAcquireExternalMemObjectsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueAcquireExternalMemObjectsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueAcquireExternalMemObjectsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueAcquireExternalMemObjectsKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueAcquireExternalMemObjectsKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueReleaseExternalMemObjectsKHRTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueReleaseExternalMemObjectsKHRTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numMemObjects,
               const cl_mem **memObjects,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numMemObjects = numMemObjects;
        params.memObjects = memObjects;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueReleaseExternalMemObjectsKHR";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReleaseExternalMemObjectsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReleaseExternalMemObjectsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReleaseExternalMemObjectsKHR)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReleaseExternalMemObjectsKHR, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueReleaseExternalMemObjectsKHRTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReleaseExternalMemObjectsKHR params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateImageTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateImageTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               const cl_image_format **imageFormat,
               const cl_image_desc **imageDesc,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.imageFormat = imageFormat;
        params.imageDesc = imageDesc;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateImage";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImage params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateImage2DTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateImage2DTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               const cl_image_format **imageFormat,
               size_t *imageWidth,
               size_t *imageHeight,
               size_t *imageRowPitch,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.imageFormat = imageFormat;
        params.imageWidth = imageWidth;
        params.imageHeight = imageHeight;
        params.imageRowPitch = imageRowPitch;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateImage2D";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage2D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage2D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateImage2DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImage2D params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateImage3DTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateImage3DTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               const cl_image_format **imageFormat,
               size_t *imageWidth,
               size_t *imageHeight,
               size_t *imageDepth,
               size_t *imageRowPitch,
               size_t *imageSlicePitch,
               void **hostPtr,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.imageFormat = imageFormat;
        params.imageWidth = imageWidth;
        params.imageHeight = imageHeight;
        params.imageDepth = imageDepth;
        params.imageRowPitch = imageRowPitch;
        params.imageSlicePitch = imageSlicePitch;
        params.hostPtr = hostPtr;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateImage3D";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage3D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage3D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateImage3DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImage3D params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateKernelTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateKernelTracer() {}

    void enter(cl_program *program,
               const char **kernelName,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.kernelName = kernelName;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_kernel *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateKernel params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateKernelsInProgramTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateKernelsInProgramTracer() {}

    void enter(cl_program *program,
               cl_uint *numKernels,
               cl_kernel **kernels,
               cl_uint **numKernelsRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.numKernels = numKernels;
        params.kernels = kernels;
        params.numKernelsRet = numKernelsRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateKernelsInProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernelsInProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernelsInProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernelsInProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernelsInProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateKernelsInProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateKernelsInProgram params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateSubDevicesTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateSubDevicesTracer() {}

    void enter(cl_device_id *inDevice,
               const cl_device_partition_property **properties,
               cl_uint *numDevices,
               cl_device_id **outDevices,
               cl_uint **numDevicesRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.inDevice = inDevice;
        params.properties = properties;
        params.numDevices = numDevices;
        params.outDevices = outDevices;
        params.numDevicesRet = numDevicesRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateSubDevices";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubDevices)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubDevices, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubDevices)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubDevices, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateSubDevicesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSubDevices params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreatePipeTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreatePipeTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               cl_uint *pipePacketSize,
               cl_uint *pipeMaxPackets,
               const cl_pipe_properties **properties,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.pipePacketSize = pipePacketSize;
        params.pipeMaxPackets = pipeMaxPackets;
        params.properties = properties;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreatePipe";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreatePipe)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreatePipe, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreatePipe)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreatePipe, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreatePipeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreatePipe params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateProgramWithBinaryTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateProgramWithBinaryTracer() {}

    void enter(cl_context *context,
               cl_uint *numDevices,
               const cl_device_id **deviceList,
               const size_t **lengths,
               const unsigned char ***binaries,
               cl_int **binaryStatus,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.numDevices = numDevices;
        params.deviceList = deviceList;
        params.lengths = lengths;
        params.binaries = binaries;
        params.binaryStatus = binaryStatus;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateProgramWithBinary";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBinary)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBinary, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBinary)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBinary, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateProgramWithBinaryTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithBinary params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateProgramWithBuiltInKernelsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateProgramWithBuiltInKernelsTracer() {}

    void enter(cl_context *context,
               cl_uint *numDevices,
               const cl_device_id **deviceList,
               const char **kernelNames,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.numDevices = numDevices;
        params.deviceList = deviceList;
        params.kernelNames = kernelNames;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateProgramWithBuiltInKernels";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBuiltInKernels)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBuiltInKernels, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBuiltInKernels)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBuiltInKernels, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateProgramWithBuiltInKernelsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithBuiltInKernels params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateProgramWithIlTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateProgramWithIlTracer() {}

    void enter(cl_context *context,
               const void **il,
               size_t *length,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.il = il;
        params.length = length;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateProgramWithIL";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithIL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithIL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithIL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithIL, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateProgramWithIlTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithIL params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateProgramWithSourceTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateProgramWithSourceTracer() {}

    void enter(cl_context *context,
               cl_uint *count,
               const char ***strings,
               const size_t **lengths,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.count = count;
        params.strings = strings;
        params.lengths = lengths;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateProgramWithSource";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithSource)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithSource, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithSource)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithSource, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateProgramWithSourceTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithSource params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateSamplerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateSamplerTracer() {}

    void enter(cl_context *context,
               cl_bool *normalizedCoords,
               cl_addressing_mode *addressingMode,
               cl_filter_mode *filterMode,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.normalizedCoords = normalizedCoords;
        params.addressingMode = addressingMode;
        params.filterMode = filterMode;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateSampler";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSampler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_sampler *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSampler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateSamplerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSampler params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateSamplerWithPropertiesTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateSamplerWithPropertiesTracer() {}

    void enter(cl_context *context,
               const cl_sampler_properties **samplerProperties,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.samplerProperties = samplerProperties;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateSamplerWithProperties";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSamplerWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSamplerWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_sampler *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSamplerWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSamplerWithProperties, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateSamplerWithPropertiesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSamplerWithProperties params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateSubBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateSubBufferTracer() {}

    void enter(cl_mem *buffer,
               cl_mem_flags *flags,
               cl_buffer_create_type *bufferCreateType,
               const void **bufferCreateInfo,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.buffer = buffer;
        params.flags = flags;
        params.bufferCreateType = bufferCreateType;
        params.bufferCreateInfo = bufferCreateInfo;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateSubBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateSubBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSubBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateUserEventTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateUserEventTracer() {}

    void enter(cl_context *context,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateUserEvent";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateUserEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateUserEvent, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_event *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateUserEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateUserEvent, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateUserEventTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateUserEvent params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueBarrierTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueBarrierTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueBarrier";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrier)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrier, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrier)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrier, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueBarrierTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueBarrier params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueBarrierWithWaitListTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueBarrierWithWaitListTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueBarrierWithWaitList";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrierWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrierWithWaitList, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrierWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrierWithWaitList, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueBarrierWithWaitListTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueBarrierWithWaitList params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueCopyBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueCopyBufferTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *srcBuffer,
               cl_mem *dstBuffer,
               size_t *srcOffset,
               size_t *dstOffset,
               size_t *cb,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.srcBuffer = srcBuffer;
        params.dstBuffer = dstBuffer;
        params.srcOffset = srcOffset;
        params.dstOffset = dstOffset;
        params.cb = cb;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueCopyBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueCopyBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueCopyBufferRectTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueCopyBufferRectTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *srcBuffer,
               cl_mem *dstBuffer,
               const size_t **srcOrigin,
               const size_t **dstOrigin,
               const size_t **region,
               size_t *srcRowPitch,
               size_t *srcSlicePitch,
               size_t *dstRowPitch,
               size_t *dstSlicePitch,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.srcBuffer = srcBuffer;
        params.dstBuffer = dstBuffer;
        params.srcOrigin = srcOrigin;
        params.dstOrigin = dstOrigin;
        params.region = region;
        params.srcRowPitch = srcRowPitch;
        params.srcSlicePitch = srcSlicePitch;
        params.dstRowPitch = dstRowPitch;
        params.dstSlicePitch = dstSlicePitch;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueCopyBufferRect";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferRect, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferRect, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueCopyBufferRectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyBufferRect params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueCopyBufferToImageTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueCopyBufferToImageTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *srcBuffer,
               cl_mem *dstImage,
               size_t *srcOffset,
               const size_t **dstOrigin,
               const size_t **region,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.srcBuffer = srcBuffer;
        params.dstImage = dstImage;
        params.srcOffset = srcOffset;
        params.dstOrigin = dstOrigin;
        params.region = region;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueCopyBufferToImage";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferToImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferToImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferToImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferToImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueCopyBufferToImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyBufferToImage params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueCopyImageTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueCopyImageTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *srcImage,
               cl_mem *dstImage,
               const size_t **srcOrigin,
               const size_t **dstOrigin,
               const size_t **region,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.srcImage = srcImage;
        params.dstImage = dstImage;
        params.srcOrigin = srcOrigin;
        params.dstOrigin = dstOrigin;
        params.region = region;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueCopyImage";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueCopyImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyImage params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueCopyImageToBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueCopyImageToBufferTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *srcImage,
               cl_mem *dstBuffer,
               const size_t **srcOrigin,
               const size_t **region,
               const size_t *dstOffset,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.srcImage = srcImage;
        params.dstBuffer = dstBuffer;
        params.srcOrigin = srcOrigin;
        params.region = region;
        params.dstOffset = dstOffset;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueCopyImageToBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImageToBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImageToBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImageToBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImageToBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueCopyImageToBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyImageToBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueFillBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueFillBufferTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *buffer,
               const void **pattern,
               size_t *patternSize,
               size_t *offset,
               size_t *size,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.buffer = buffer;
        params.pattern = pattern;
        params.patternSize = patternSize;
        params.offset = offset;
        params.size = size;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueFillBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueFillBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueFillBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueFillImageTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueFillImageTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *image,
               const void **fillColor,
               const size_t **origin,
               const size_t **region,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.image = image;
        params.fillColor = fillColor;
        params.origin = origin;
        params.region = region;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueFillImage";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueFillImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueFillImage params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMapBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMapBufferTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *buffer,
               cl_bool *blockingMap,
               cl_map_flags *mapFlags,
               size_t *offset,
               size_t *cb,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.buffer = buffer;
        params.blockingMap = blockingMap;
        params.mapFlags = mapFlags;
        params.offset = offset;
        params.cb = cb;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMapBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMapBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMapBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMapImageTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMapImageTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *image,
               cl_bool *blockingMap,
               cl_map_flags *mapFlags,
               const size_t **origin,
               const size_t **region,
               size_t **imageRowPitch,
               size_t **imageSlicePitch,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.image = image;
        params.blockingMap = blockingMap;
        params.mapFlags = mapFlags;
        params.origin = origin;
        params.region = region;
        params.imageRowPitch = imageRowPitch;
        params.imageSlicePitch = imageSlicePitch;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMapImage";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMapImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMapImage params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMarkerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMarkerTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMarker";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarker)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarker, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarker)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarker, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMarkerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMarker params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMarkerWithWaitListTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMarkerWithWaitListTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMarkerWithWaitList";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarkerWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarkerWithWaitList, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarkerWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarkerWithWaitList, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMarkerWithWaitListTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMarkerWithWaitList params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueMigrateMemObjectsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueMigrateMemObjectsTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numMemObjects,
               const cl_mem **memObjects,
               cl_mem_migration_flags *flags,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numMemObjects = numMemObjects;
        params.memObjects = memObjects;
        params.flags = flags;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueMigrateMemObjects";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMigrateMemObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMigrateMemObjects, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMigrateMemObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMigrateMemObjects, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueMigrateMemObjectsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMigrateMemObjects params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueNdRangeKernelTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueNdRangeKernelTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_kernel *kernel,
               cl_uint *workDim,
               const size_t **globalWorkOffset,
               const size_t **globalWorkSize,
               const size_t **localWorkSize,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.kernel = kernel;
        params.workDim = workDim;
        params.globalWorkOffset = globalWorkOffset;
        params.globalWorkSize = globalWorkSize;
        params.localWorkSize = localWorkSize;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueNDRangeKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNDRangeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNDRangeKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNDRangeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNDRangeKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueNdRangeKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueNDRangeKernel params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueNativeKernelTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueNativeKernelTracer() {}

    void enter(cl_command_queue *commandQueue,
               void(CL_CALLBACK **userFunc)(void *),
               void **args,
               size_t *cbArgs,
               cl_uint *numMemObjects,
               const cl_mem **memList,
               const void ***argsMemLoc,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.userFunc = userFunc;
        params.args = args;
        params.cbArgs = cbArgs;
        params.numMemObjects = numMemObjects;
        params.memList = memList;
        params.argsMemLoc = argsMemLoc;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueNativeKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNativeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNativeKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNativeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNativeKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueNativeKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueNativeKernel params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueReadBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueReadBufferTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *buffer,
               cl_bool *blockingRead,
               size_t *offset,
               size_t *cb,
               void **ptr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.buffer = buffer;
        params.blockingRead = blockingRead;
        params.offset = offset;
        params.cb = cb;
        params.ptr = ptr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueReadBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueReadBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReadBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueReadBufferRectTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueReadBufferRectTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *buffer,
               cl_bool *blockingRead,
               const size_t **bufferOrigin,
               const size_t **hostOrigin,
               const size_t **region,
               size_t *bufferRowPitch,
               size_t *bufferSlicePitch,
               size_t *hostRowPitch,
               size_t *hostSlicePitch,
               void **ptr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.buffer = buffer;
        params.blockingRead = blockingRead;
        params.bufferOrigin = bufferOrigin;
        params.hostOrigin = hostOrigin;
        params.region = region;
        params.bufferRowPitch = bufferRowPitch;
        params.bufferSlicePitch = bufferSlicePitch;
        params.hostRowPitch = hostRowPitch;
        params.hostSlicePitch = hostSlicePitch;
        params.ptr = ptr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueReadBufferRect";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBufferRect, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBufferRect, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueReadBufferRectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReadBufferRect params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueReadImageTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueReadImageTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *image,
               cl_bool *blockingRead,
               const size_t **origin,
               const size_t **region,
               size_t *rowPitch,
               size_t *slicePitch,
               void **ptr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.image = image;
        params.blockingRead = blockingRead;
        params.origin = origin;
        params.region = region;
        params.rowPitch = rowPitch;
        params.slicePitch = slicePitch;
        params.ptr = ptr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueReadImage";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueReadImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReadImage params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueSvmFreeTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueSvmFreeTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numSvmPointers,
               void ***svmPointers,
               void(CL_CALLBACK **pfnFreeFunc)(cl_command_queue queue, cl_uint numSvmPointers, void **svmPointers, void *userData),
               void **userData,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numSvmPointers = numSvmPointers;
        params.svmPointers = svmPointers;
        params.pfnFreeFunc = pfnFreeFunc;
        params.userData = userData;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueSVMFree";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMFree, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMFree, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueSvmFreeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMFree params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueSvmMapTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueSvmMapTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_bool *blockingMap,
               cl_map_flags *mapFlags,
               void **svmPtr,
               size_t *size,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.blockingMap = blockingMap;
        params.mapFlags = mapFlags;
        params.svmPtr = svmPtr;
        params.size = size;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueSVMMap";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMap, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMap, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueSvmMapTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMap params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueSvmMemFillTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueSvmMemFillTracer() {}

    void enter(cl_command_queue *commandQueue,
               void **svmPtr,
               const void **pattern,
               size_t *patternSize,
               size_t *size,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.svmPtr = svmPtr;
        params.pattern = pattern;
        params.patternSize = patternSize;
        params.size = size;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueSVMMemFill";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemFill)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemFill, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemFill)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemFill, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueSvmMemFillTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMemFill params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueSvmMemcpyTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueSvmMemcpyTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_bool *blockingCopy,
               void **dstPtr,
               const void **srcPtr,
               size_t *size,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.blockingCopy = blockingCopy;
        params.dstPtr = dstPtr;
        params.srcPtr = srcPtr;
        params.size = size;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueSVMMemcpy";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemcpy)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemcpy, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemcpy)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemcpy, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueSvmMemcpyTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMemcpy params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueSvmMigrateMemTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueSvmMigrateMemTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numSvmPointers,
               const void ***svmPointers,
               const size_t **sizes,
               const cl_mem_migration_flags *flags,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numSvmPointers = numSvmPointers;
        params.svmPointers = svmPointers;
        params.sizes = sizes;
        params.flags = flags;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueSVMMigrateMem";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMigrateMem)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMigrateMem, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMigrateMem)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMigrateMem, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueSvmMigrateMemTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMigrateMem params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueSvmUnmapTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueSvmUnmapTracer() {}

    void enter(cl_command_queue *commandQueue,
               void **svmPtr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.svmPtr = svmPtr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueSVMUnmap";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMUnmap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMUnmap, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMUnmap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMUnmap, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueSvmUnmapTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMUnmap params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueTaskTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueTaskTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_kernel *kernel,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.kernel = kernel;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueTask";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueTask)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueTask, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueTask)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueTask, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueTaskTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueTask params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueUnmapMemObjectTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueUnmapMemObjectTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *memobj,
               void **mappedPtr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.memobj = memobj;
        params.mappedPtr = mappedPtr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueUnmapMemObject";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueUnmapMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueUnmapMemObject, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueUnmapMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueUnmapMemObject, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueUnmapMemObjectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueUnmapMemObject params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueWaitForEventsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueWaitForEventsTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numEvents,
               const cl_event **eventList) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numEvents = numEvents;
        params.eventList = eventList;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueWaitForEvents";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWaitForEvents, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWaitForEvents, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueWaitForEventsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWaitForEvents params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueWriteBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueWriteBufferTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *buffer,
               cl_bool *blockingWrite,
               size_t *offset,
               size_t *cb,
               const void **ptr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.buffer = buffer;
        params.blockingWrite = blockingWrite;
        params.offset = offset;
        params.cb = cb;
        params.ptr = ptr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueWriteBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueWriteBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWriteBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueWriteBufferRectTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueWriteBufferRectTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *buffer,
               cl_bool *blockingWrite,
               const size_t **bufferOrigin,
               const size_t **hostOrigin,
               const size_t **region,
               size_t *bufferRowPitch,
               size_t *bufferSlicePitch,
               size_t *hostRowPitch,
               size_t *hostSlicePitch,
               const void **ptr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.buffer = buffer;
        params.blockingWrite = blockingWrite;
        params.bufferOrigin = bufferOrigin;
        params.hostOrigin = hostOrigin;
        params.region = region;
        params.bufferRowPitch = bufferRowPitch;
        params.bufferSlicePitch = bufferSlicePitch;
        params.hostRowPitch = hostRowPitch;
        params.hostSlicePitch = hostSlicePitch;
        params.ptr = ptr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueWriteBufferRect";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBufferRect, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBufferRect, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueWriteBufferRectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWriteBufferRect params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueWriteImageTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueWriteImageTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *image,
               cl_bool *blockingWrite,
               const size_t **origin,
               const size_t **region,
               size_t *inputRowPitch,
               size_t *inputSlicePitch,
               const void **ptr,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.image = image;
        params.blockingWrite = blockingWrite;
        params.origin = origin;
        params.region = region;
        params.inputRowPitch = inputRowPitch;
        params.inputSlicePitch = inputSlicePitch;
        params.ptr = ptr;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueWriteImage";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteImage, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueWriteImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWriteImage params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClFinishTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClFinishTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clFinish";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFinish)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFinish, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFinish)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFinish, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClFinishTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clFinish params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClFlushTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClFlushTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clFlush";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFlush)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFlush, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFlush)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFlush, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClFlushTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clFlush params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetCommandQueueInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetCommandQueueInfoTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_command_queue_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetCommandQueueInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetCommandQueueInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetCommandQueueInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetCommandQueueInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetCommandQueueInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetCommandQueueInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetCommandQueueInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetContextInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetContextInfoTracer() {}

    void enter(cl_context *context,
               cl_context_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetContextInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetContextInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetContextInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetContextInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetContextInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetContextInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetContextInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetDeviceAndHostTimerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetDeviceAndHostTimerTracer() {}

    void enter(cl_device_id *device,
               cl_ulong **deviceTimestamp,
               cl_ulong **hostTimestamp) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;
        params.deviceTimestamp = deviceTimestamp;
        params.hostTimestamp = hostTimestamp;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetDeviceAndHostTimer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceAndHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceAndHostTimer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceAndHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceAndHostTimer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetDeviceAndHostTimerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceAndHostTimer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetDeviceIDsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetDeviceIDsTracer() {}

    void enter(cl_platform_id *platform,
               cl_device_type *deviceType,
               cl_uint *numEntries,
               cl_device_id **devices,
               cl_uint **numDevices) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.platform = platform;
        params.deviceType = deviceType;
        params.numEntries = numEntries;
        params.devices = devices;
        params.numDevices = numDevices;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetDeviceIDs";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceIDs, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceIDs, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetDeviceIDsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceIDs params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetDeviceInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetDeviceInfoTracer() {}

    void enter(cl_device_id *device,
               cl_device_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetDeviceInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetDeviceInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetEventInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetEventInfoTracer() {}

    void enter(cl_event *event,
               cl_event_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetEventInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetEventInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetEventInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetEventProfilingInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetEventProfilingInfoTracer() {}

    void enter(cl_event *event,
               cl_profiling_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetEventProfilingInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventProfilingInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventProfilingInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventProfilingInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventProfilingInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetEventProfilingInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetEventProfilingInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetExtensionFunctionAddressTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetExtensionFunctionAddressTracer() {}

    void enter(const char **funcName) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.funcName = funcName;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetExtensionFunctionAddress";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddress)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddress, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddress)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddress, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetExtensionFunctionAddressTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetExtensionFunctionAddress params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetExtensionFunctionAddressForPlatformTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetExtensionFunctionAddressForPlatformTracer() {}

    void enter(cl_platform_id *platform,
               const char **funcName) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.platform = platform;
        params.funcName = funcName;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetExtensionFunctionAddressForPlatform";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetExtensionFunctionAddressForPlatformTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetExtensionFunctionAddressForPlatform params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetHostTimerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetHostTimerTracer() {}

    void enter(cl_device_id *device,
               cl_ulong **hostTimestamp) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;
        params.hostTimestamp = hostTimestamp;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetHostTimer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetHostTimer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetHostTimer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetHostTimerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetHostTimer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetImageInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetImageInfoTracer() {}

    void enter(cl_mem *image,
               cl_image_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.image = image;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetImageInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetImageInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetImageInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetImageInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetImageInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetImageInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetImageInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelArgInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelArgInfoTracer() {}

    void enter(cl_kernel *kernel,
               cl_uint *argIndx,
               cl_kernel_arg_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.argIndx = argIndx;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelArgInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelArgInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelArgInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelArgInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelArgInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelArgInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelArgInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelInfoTracer() {}

    void enter(cl_kernel *kernel,
               cl_kernel_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelSubGroupInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelSubGroupInfoTracer() {}

    void enter(cl_kernel *kernel,
               cl_device_id *device,
               cl_kernel_sub_group_info *paramName,
               size_t *inputValueSize,
               const void **inputValue,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.device = device;
        params.paramName = paramName;
        params.inputValueSize = inputValueSize;
        params.inputValue = inputValue;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelSubGroupInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSubGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSubGroupInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSubGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSubGroupInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelSubGroupInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelSubGroupInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetKernelWorkGroupInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetKernelWorkGroupInfoTracer() {}

    void enter(cl_kernel *kernel,
               cl_device_id *device,
               cl_kernel_work_group_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.device = device;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetKernelWorkGroupInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelWorkGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelWorkGroupInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelWorkGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelWorkGroupInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetKernelWorkGroupInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelWorkGroupInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetMemObjectInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetMemObjectInfoTracer() {}

    void enter(cl_mem *memobj,
               cl_mem_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetMemObjectInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetMemObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetMemObjectInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetMemObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetMemObjectInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetMemObjectInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetMemObjectInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetPipeInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetPipeInfoTracer() {}

    void enter(cl_mem *pipe,
               cl_pipe_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.pipe = pipe;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetPipeInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPipeInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPipeInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPipeInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPipeInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetPipeInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetPipeInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetPlatformIDsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetPlatformIDsTracer() {}

    void enter(cl_uint *numEntries,
               cl_platform_id **platforms,
               cl_uint **numPlatforms) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.numEntries = numEntries;
        params.platforms = platforms;
        params.numPlatforms = numPlatforms;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetPlatformIDs";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformIDs, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformIDs, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetPlatformIDsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetPlatformIDs params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetPlatformInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetPlatformInfoTracer() {}

    void enter(cl_platform_id *platform,
               cl_platform_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.platform = platform;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetPlatformInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetPlatformInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetPlatformInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetProgramBuildInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetProgramBuildInfoTracer() {}

    void enter(cl_program *program,
               cl_device_id *device,
               cl_program_build_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.device = device;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetProgramBuildInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramBuildInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramBuildInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramBuildInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramBuildInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetProgramBuildInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetProgramBuildInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetProgramInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetProgramInfoTracer() {}

    void enter(cl_program *program,
               cl_program_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetProgramInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetProgramInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetProgramInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetSamplerInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetSamplerInfoTracer() {}

    void enter(cl_sampler *sampler,
               cl_sampler_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.sampler = sampler;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetSamplerInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSamplerInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSamplerInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSamplerInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSamplerInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetSamplerInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetSamplerInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetSupportedImageFormatsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetSupportedImageFormatsTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               cl_mem_object_type *imageType,
               cl_uint *numEntries,
               cl_image_format **imageFormats,
               cl_uint **numImageFormats) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.imageType = imageType;
        params.numEntries = numEntries;
        params.imageFormats = imageFormats;
        params.numImageFormats = numImageFormats;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetSupportedImageFormats";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSupportedImageFormats)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSupportedImageFormats, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSupportedImageFormats)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSupportedImageFormats, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetSupportedImageFormatsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetSupportedImageFormats params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClLinkProgramTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClLinkProgramTracer() {}

    void enter(cl_context *context,
               cl_uint *numDevices,
               const cl_device_id **deviceList,
               const char **options,
               cl_uint *numInputPrograms,
               const cl_program **inputPrograms,
               void(CL_CALLBACK **funcNotify)(cl_program program, void *userData),
               void **userData,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.numDevices = numDevices;
        params.deviceList = deviceList;
        params.options = options;
        params.numInputPrograms = numInputPrograms;
        params.inputPrograms = inputPrograms;
        params.funcNotify = funcNotify;
        params.userData = userData;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clLinkProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clLinkProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clLinkProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clLinkProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clLinkProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClLinkProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clLinkProgram params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseCommandQueueTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseCommandQueueTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseCommandQueue";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseCommandQueue params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseContextTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseContextTracer() {}

    void enter(cl_context *context) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseContext";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseContext, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseContext, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseContextTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseContext params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseDeviceTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseDeviceTracer() {}

    void enter(cl_device_id *device) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseDevice";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseDevice, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseDevice, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseDeviceTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseDevice params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseEventTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseEventTracer() {}

    void enter(cl_event *event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseEvent";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseEvent, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseEvent, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseEventTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseEvent params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseKernelTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseKernelTracer() {}

    void enter(cl_kernel *kernel) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseKernel params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseMemObjectTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseMemObjectTracer() {}

    void enter(cl_mem *memobj) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseMemObject";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseMemObject, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseMemObject, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseMemObjectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseMemObject params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseProgramTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseProgramTracer() {}

    void enter(cl_program *program) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseProgram params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClReleaseSamplerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClReleaseSamplerTracer() {}

    void enter(cl_sampler *sampler) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.sampler = sampler;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseSampler";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseSampler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseSampler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClReleaseSamplerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseSampler params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainCommandQueueTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainCommandQueueTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainCommandQueue";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainCommandQueue params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainContextTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainContextTracer() {}

    void enter(cl_context *context) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainContext";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainContext, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainContext, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainContextTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainContext params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainDeviceTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainDeviceTracer() {}

    void enter(cl_device_id *device) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainDevice";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainDevice, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainDevice, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainDeviceTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainDevice params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainEventTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainEventTracer() {}

    void enter(cl_event *event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainEvent";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainEvent, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainEvent, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainEventTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainEvent params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainKernelTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainKernelTracer() {}

    void enter(cl_kernel *kernel) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainKernel, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainKernel params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainMemObjectTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainMemObjectTracer() {}

    void enter(cl_mem *memobj) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainMemObject";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainMemObject, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainMemObject, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainMemObjectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainMemObject params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainProgramTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainProgramTracer() {}

    void enter(cl_program *program) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainProgram, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainProgram params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClRetainSamplerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClRetainSamplerTracer() {}

    void enter(cl_sampler *sampler) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.sampler = sampler;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainSampler";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainSampler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainSampler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClRetainSamplerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainSampler params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSvmAllocTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSvmAllocTracer() {}

    void enter(cl_context *context,
               cl_svm_mem_flags *flags,
               size_t *size,
               cl_uint *alignment) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.size = size;
        params.alignment = alignment;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSVMAlloc";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMAlloc)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMAlloc, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMAlloc)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMAlloc, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSvmAllocTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSVMAlloc params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSvmFreeTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSvmFreeTracer() {}

    void enter(cl_context *context,
               void **svmPointer) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.svmPointer = svmPointer;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSVMFree";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMFree, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMFree, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSvmFreeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSVMFree params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetCommandQueuePropertyTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetCommandQueuePropertyTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_command_queue_properties *properties,
               cl_bool *enable,
               cl_command_queue_properties **oldProperties) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.properties = properties;
        params.enable = enable;
        params.oldProperties = oldProperties;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetCommandQueueProperty";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetCommandQueueProperty)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetCommandQueueProperty, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetCommandQueueProperty)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetCommandQueueProperty, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetCommandQueuePropertyTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetCommandQueueProperty params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetDefaultDeviceCommandQueueTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetDefaultDeviceCommandQueueTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.device = device;
        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetDefaultDeviceCommandQueue";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetDefaultDeviceCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetDefaultDeviceCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetDefaultDeviceCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetDefaultDeviceCommandQueue, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetDefaultDeviceCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetDefaultDeviceCommandQueue params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetEventCallbackTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetEventCallbackTracer() {}

    void enter(cl_event *event,
               cl_int *commandExecCallbackType,
               void(CL_CALLBACK **funcNotify)(cl_event, cl_int, void *),
               void **userData) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;
        params.commandExecCallbackType = commandExecCallbackType;
        params.funcNotify = funcNotify;
        params.userData = userData;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetEventCallback";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetEventCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetEventCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetEventCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetEventCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetEventCallbackTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetEventCallback params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetKernelArgTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetKernelArgTracer() {}

    void enter(cl_kernel *kernel,
               cl_uint *argIndex,
               size_t *argSize,
               const void **argValue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.argIndex = argIndex;
        params.argSize = argSize;
        params.argValue = argValue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetKernelArg";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArg)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArg, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArg)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArg, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetKernelArgTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetKernelArg params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetKernelArgSvmPointerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetKernelArgSvmPointerTracer() {}

    void enter(cl_kernel *kernel,
               cl_uint *argIndex,
               const void **argValue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.argIndex = argIndex;
        params.argValue = argValue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetKernelArgSVMPointer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArgSVMPointer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArgSVMPointer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArgSVMPointer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArgSVMPointer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetKernelArgSvmPointerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetKernelArgSVMPointer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetKernelExecInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetKernelExecInfoTracer() {}

    void enter(cl_kernel *kernel,
               cl_kernel_exec_info *paramName,
               size_t *paramValueSize,
               const void **paramValue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetKernelExecInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelExecInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelExecInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelExecInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelExecInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetKernelExecInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetKernelExecInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetMemObjectDestructorCallbackTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetMemObjectDestructorCallbackTracer() {}

    void enter(cl_mem *memobj,
               void(CL_CALLBACK **funcNotify)(cl_mem, void *),
               void **userData) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;
        params.funcNotify = funcNotify;
        params.userData = userData;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetMemObjectDestructorCallback";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetMemObjectDestructorCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetMemObjectDestructorCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetMemObjectDestructorCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetMemObjectDestructorCallback, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetMemObjectDestructorCallbackTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetMemObjectDestructorCallback params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClSetUserEventStatusTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClSetUserEventStatusTracer() {}

    void enter(cl_event *event,
               cl_int *executionStatus) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;
        params.executionStatus = executionStatus;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clSetUserEventStatus";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetUserEventStatus)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetUserEventStatus, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetUserEventStatus)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetUserEventStatus, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClSetUserEventStatusTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetUserEventStatus params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClUnloadCompilerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClUnloadCompilerTracer() {}

    void enter() {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clUnloadCompiler";
        data.functionParams = nullptr;
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadCompiler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadCompiler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClUnloadCompilerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClUnloadPlatformCompilerTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClUnloadPlatformCompilerTracer() {}

    void enter(cl_platform_id *platform) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.platform = platform;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clUnloadPlatformCompiler";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadPlatformCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadPlatformCompiler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadPlatformCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadPlatformCompiler, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClUnloadPlatformCompilerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clUnloadPlatformCompiler params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClWaitForEventsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClWaitForEventsTracer() {}

    void enter(cl_uint *numEvents,
               const cl_event **eventList) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.numEvents = numEvents;
        params.eventList = eventList;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clWaitForEvents";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clWaitForEvents, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clWaitForEvents, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClWaitForEventsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clWaitForEvents params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateFromGlBufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateFromGlBufferTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               cl_GLuint *bufobj,
               int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.bufobj = bufobj;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateFromGLBuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLBuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateFromGlBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLBuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateFromGlRenderbufferTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateFromGlRenderbufferTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               cl_GLuint *renderbuffer,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.renderbuffer = renderbuffer;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateFromGLRenderbuffer";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLRenderbuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLRenderbuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLRenderbuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLRenderbuffer, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateFromGlRenderbufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLRenderbuffer params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateFromGlTextureTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateFromGlTextureTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               cl_GLenum *target,
               cl_GLint *miplevel,
               cl_GLuint *texture,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.target = target;
        params.miplevel = miplevel;
        params.texture = texture;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateFromGLTexture";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateFromGlTextureTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLTexture params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateFromGlTexture2DTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateFromGlTexture2DTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               cl_GLenum *target,
               cl_GLint *miplevel,
               cl_GLuint *texture,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.target = target;
        params.miplevel = miplevel;
        params.texture = texture;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateFromGLTexture2D";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture2D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture2D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateFromGlTexture2DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLTexture2D params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClCreateFromGlTexture3DTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClCreateFromGlTexture3DTracer() {}

    void enter(cl_context *context,
               cl_mem_flags *flags,
               cl_GLenum *target,
               cl_GLint *miplevel,
               cl_GLuint *texture,
               cl_int **errcodeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;
        params.flags = flags;
        params.target = target;
        params.miplevel = miplevel;
        params.texture = texture;
        params.errcodeRet = errcodeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clCreateFromGLTexture3D";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture3D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture3D, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClCreateFromGlTexture3DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLTexture3D params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueAcquireGlObjectsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueAcquireGlObjectsTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numObjects,
               const cl_mem **memObjects,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numObjects = numObjects;
        params.memObjects = memObjects;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueAcquireGLObjects";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueAcquireGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueAcquireGLObjects, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueAcquireGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueAcquireGLObjects, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueAcquireGlObjectsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueAcquireGLObjects params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClEnqueueReleaseGlObjectsTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClEnqueueReleaseGlObjectsTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_uint *numObjects,
               const cl_mem **memObjects,
               cl_uint *numEventsInWaitList,
               const cl_event **eventWaitList,
               cl_event **event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;
        params.numObjects = numObjects;
        params.memObjects = memObjects;
        params.numEventsInWaitList = numEventsInWaitList;
        params.eventWaitList = eventWaitList;
        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueReleaseGLObjects";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReleaseGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReleaseGLObjects, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReleaseGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReleaseGLObjects, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClEnqueueReleaseGlObjectsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReleaseGLObjects params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetGlObjectInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetGlObjectInfoTracer() {}

    void enter(cl_mem *memobj,
               cl_gl_object_type **glObjectType,
               cl_GLuint **glObjectName) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;
        params.glObjectType = glObjectType;
        params.glObjectName = glObjectName;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetGLObjectInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLObjectInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLObjectInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetGlObjectInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetGLObjectInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class ClGetGlTextureInfoTracer : NEO::NonCopyableAndNonMovableClass {
  public:
    ClGetGlTextureInfoTracer() {}

    void enter(cl_mem *memobj,
               cl_gl_texture_info *paramName,
               size_t *paramValueSize,
               void **paramValue,
               size_t **paramValueSizeRet) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;
        params.paramName = paramName;
        params.paramValueSize = paramValueSize;
        params.paramValue = paramValue;
        params.paramValueSizeRet = paramValueSizeRet;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetGLTextureInfo";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLTextureInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLTextureInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        size_t i = 0;
        DEBUG_BREAK_IF(tracingHandle[0] == nullptr);
        while (i < tracingMaxHandleCount && tracingHandle[i] != nullptr) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLTextureInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLTextureInfo, &data);
            }
            ++i;
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~ClGetGlTextureInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetGLTextureInfo params{};
    cl_callback_data data{};
    uint64_t correlationData[tracingMaxHandleCount];
    TracingNotifyState state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

static_assert(NEO::NonCopyableAndNonMovable<ClBuildProgramTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCloneKernelTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCompileProgramTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateCommandQueueTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateCommandQueueWithPropertiesTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateContextTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateContextFromTypeTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClMemFreeINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClIcdGetPlatformIDsKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateBufferWithPropertiesINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateBufferWithPropertiesTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateImageWithPropertiesTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateImageWithPropertiesINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetImageParamsINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreatePerfCountersCommandQueueINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClHostMemAllocINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClDeviceMemAllocINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSharedMemAllocINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClMemBlockingFreeINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetMemAllocInfoINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetKernelArgMemPointerINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMemsetINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMemFillINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMemcpyINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMigrateMemINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMemAdviseINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateCommandQueueWithPropertiesKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateAcceleratorINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainAcceleratorINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetAcceleratorInfoINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseAcceleratorINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateProgramWithILKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelSuggestedLocalWorkSizeKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelSubGroupInfoKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueVerifyMemoryINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClAddCommentINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetDeviceGlobalVariablePointerINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetDeviceFunctionPointerINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetProgramReleaseCallbackTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetProgramSpecializationConstantTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelSuggestedLocalWorkSizeINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelMaxConcurrentWorkGroupCountINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueNDCountKernelINTELTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetContextDestructorCallbackTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueExternalMemObjectsKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueAcquireExternalMemObjectsKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueReleaseExternalMemObjectsKHRTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateImageTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateImage2DTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateImage3DTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateKernelTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateKernelsInProgramTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateSubDevicesTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreatePipeTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateProgramWithBinaryTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateProgramWithBuiltInKernelsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateProgramWithIlTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateProgramWithSourceTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateSamplerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateSamplerWithPropertiesTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateSubBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateUserEventTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueBarrierTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueBarrierWithWaitListTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueCopyBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueCopyBufferRectTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueCopyBufferToImageTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueCopyImageTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueCopyImageToBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueFillBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueFillImageTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMapBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMapImageTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMarkerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMarkerWithWaitListTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueMigrateMemObjectsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueNdRangeKernelTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueNativeKernelTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueReadBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueReadBufferRectTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueReadImageTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueSvmFreeTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueSvmMapTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueSvmMemFillTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueSvmMemcpyTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueSvmMigrateMemTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueSvmUnmapTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueTaskTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueUnmapMemObjectTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueWaitForEventsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueWriteBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueWriteBufferRectTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueWriteImageTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClFinishTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClFlushTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetCommandQueueInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetContextInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetDeviceAndHostTimerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetDeviceIDsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetDeviceInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetEventInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetEventProfilingInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetExtensionFunctionAddressTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetExtensionFunctionAddressForPlatformTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetHostTimerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetImageInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelArgInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelSubGroupInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetKernelWorkGroupInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetMemObjectInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetPipeInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetPlatformIDsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetPlatformInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetProgramBuildInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetProgramInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetSamplerInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetSupportedImageFormatsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClLinkProgramTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseCommandQueueTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseContextTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseDeviceTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseEventTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseKernelTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseMemObjectTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseProgramTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClReleaseSamplerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainCommandQueueTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainContextTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainDeviceTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainEventTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainKernelTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainMemObjectTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainProgramTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClRetainSamplerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSvmAllocTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSvmFreeTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetCommandQueuePropertyTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetDefaultDeviceCommandQueueTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetEventCallbackTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetKernelArgTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetKernelArgSvmPointerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetKernelExecInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetMemObjectDestructorCallbackTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClSetUserEventStatusTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClUnloadCompilerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClUnloadPlatformCompilerTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClWaitForEventsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateFromGlBufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateFromGlRenderbufferTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateFromGlTextureTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateFromGlTexture2DTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClCreateFromGlTexture3DTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueAcquireGlObjectsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClEnqueueReleaseGlObjectsTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetGlObjectInfoTracer>);
static_assert(NEO::NonCopyableAndNonMovable<ClGetGlTextureInfoTracer>);

} // namespace HostSideTracing
