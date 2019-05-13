/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/tracing/tracing_handle.h"

#include <atomic>
#include <immintrin.h>
#include <thread>
#include <vector>

namespace HostSideTracing {

#define TRACING_SET_ENABLED_BIT(state) ((state) | (HostSideTracing::TRACING_STATE_ENABLED_BIT))
#define TRACING_UNSET_ENABLED_BIT(state) ((state) & (~HostSideTracing::TRACING_STATE_ENABLED_BIT))
#define TRACING_GET_ENABLED_BIT(state) ((state) & (HostSideTracing::TRACING_STATE_ENABLED_BIT))

#define TRACING_SET_LOCKED_BIT(state) ((state) | (HostSideTracing::TRACING_STATE_LOCKED_BIT))
#define TRACING_UNSET_LOCKED_BIT(state) ((state) & (~HostSideTracing::TRACING_STATE_LOCKED_BIT))
#define TRACING_GET_LOCKED_BIT(state) ((state) & (HostSideTracing::TRACING_STATE_LOCKED_BIT))

#define TRACING_ZERO_CLIENT_COUNTER(state) ((state) & (HostSideTracing::TRACING_STATE_ENABLED_BIT | HostSideTracing::TRACING_STATE_LOCKED_BIT))
#define TRACING_GET_CLIENT_COUNTER(state) ((state) & (~(HostSideTracing::TRACING_STATE_ENABLED_BIT | HostSideTracing::TRACING_STATE_LOCKED_BIT)))

#define TRACING_ENTER(name, ...)                                                                  \
    bool isHostSideTracingEnabled_##name = false;                                                 \
    HostSideTracing::name##Tracer tracer_##name;                                                  \
    if (TRACING_GET_ENABLED_BIT(HostSideTracing::tracingState.load(std::memory_order_acquire))) { \
        isHostSideTracingEnabled_##name = HostSideTracing::addTracingClient();                    \
        if (isHostSideTracingEnabled_##name) {                                                    \
            tracer_##name.enter(__VA_ARGS__);                                                     \
        }                                                                                         \
    }

#define TRACING_EXIT(name, ...)                 \
    if (isHostSideTracingEnabled_##name) {      \
        tracer_##name.exit(__VA_ARGS__);        \
        HostSideTracing::removeTracingClient(); \
    }

typedef enum _tracing_notify_state_t {
    TRACING_NOTIFY_STATE_NOTHING_CALLED = 0,
    TRACING_NOTIFY_STATE_ENTER_CALLED = 1,
    TRACING_NOTIFY_STATE_EXIT_CALLED = 2,
} tracing_notify_state_t;

constexpr size_t TRACING_MAX_HANDLE_COUNT = 16;

constexpr uint32_t TRACING_STATE_ENABLED_BIT = 0x80000000u;
constexpr uint32_t TRACING_STATE_LOCKED_BIT = 0x40000000u;

extern std::atomic<uint32_t> tracingState;
extern std::vector<TracingHandle *> tracingHandle;
extern std::atomic<uint32_t> tracingCorrelationId;

bool addTracingClient();
void removeTracingClient();

class AtomicBackoff {
  public:
    AtomicBackoff() {}

    void pause() {
        if (count < loopsBeforeYield) {
            for (uint32_t i = 0; i < count; i++) {
                _mm_pause();
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

class clBuildProgramTracer {
  public:
    clBuildProgramTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clBuildProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clBuildProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clBuildProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clBuildProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clBuildProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clBuildProgram params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCloneKernelTracer {
  public:
    clCloneKernelTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCloneKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCloneKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_kernel *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCloneKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCloneKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCloneKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCloneKernel params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCompileProgramTracer {
  public:
    clCompileProgramTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCompileProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCompileProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCompileProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCompileProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCompileProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCompileProgram params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateBufferTracer {
  public:
    clCreateBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateCommandQueueTracer {
  public:
    clCreateCommandQueueTracer() {}

    void enter(cl_context *context,
               cl_device_id *device,
               cl_command_queue_properties *properties,
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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_command_queue *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateCommandQueue params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateCommandQueueWithPropertiesTracer {
  public:
    clCreateCommandQueueWithPropertiesTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueueWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueueWithProperties, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_command_queue *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateCommandQueueWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateCommandQueueWithProperties, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateCommandQueueWithPropertiesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateCommandQueueWithProperties params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateContextTracer {
  public:
    clCreateContextTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContext, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_context *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContext, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateContextTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateContext params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateContextFromTypeTracer {
  public:
    clCreateContextFromTypeTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContextFromType)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContextFromType, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_context *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateContextFromType)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateContextFromType, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateContextFromTypeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateContextFromType params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateImageTracer {
  public:
    clCreateImageTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImage params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateImage2DTracer {
  public:
    clCreateImage2DTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage2D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage2D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateImage2DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImage2D params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateImage3DTracer {
  public:
    clCreateImage3DTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage3D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateImage3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateImage3D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateImage3DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateImage3D params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateKernelTracer {
  public:
    clCreateKernelTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_kernel *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateKernel params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateKernelsInProgramTracer {
  public:
    clCreateKernelsInProgramTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernelsInProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernelsInProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateKernelsInProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateKernelsInProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateKernelsInProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateKernelsInProgram params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreatePipeTracer {
  public:
    clCreatePipeTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreatePipe)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreatePipe, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreatePipe)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreatePipe, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreatePipeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreatePipe params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateProgramWithBinaryTracer {
  public:
    clCreateProgramWithBinaryTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBinary)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBinary, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBinary)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBinary, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateProgramWithBinaryTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithBinary params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateProgramWithBuiltInKernelsTracer {
  public:
    clCreateProgramWithBuiltInKernelsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBuiltInKernels)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBuiltInKernels, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithBuiltInKernels)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithBuiltInKernels, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateProgramWithBuiltInKernelsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithBuiltInKernels params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateProgramWithILTracer {
  public:
    clCreateProgramWithILTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithIL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithIL, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithIL)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithIL, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateProgramWithILTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithIL params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateProgramWithSourceTracer {
  public:
    clCreateProgramWithSourceTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithSource)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithSource, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateProgramWithSource)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateProgramWithSource, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateProgramWithSourceTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateProgramWithSource params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateSamplerTracer {
  public:
    clCreateSamplerTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSampler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_sampler *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSampler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateSamplerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSampler params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateSamplerWithPropertiesTracer {
  public:
    clCreateSamplerWithPropertiesTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSamplerWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSamplerWithProperties, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_sampler *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSamplerWithProperties)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSamplerWithProperties, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateSamplerWithPropertiesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSamplerWithProperties params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateSubBufferTracer {
  public:
    clCreateSubBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateSubBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSubBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateSubDevicesTracer {
  public:
    clCreateSubDevicesTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubDevices)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubDevices, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateSubDevices)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateSubDevices, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateSubDevicesTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateSubDevices params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateUserEventTracer {
  public:
    clCreateUserEventTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateUserEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateUserEvent, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_event *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateUserEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateUserEvent, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateUserEventTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateUserEvent params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueBarrierTracer {
  public:
    clEnqueueBarrierTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clEnqueueBarrier";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrier)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrier, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrier)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrier, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueBarrierTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueBarrier params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueBarrierWithWaitListTracer {
  public:
    clEnqueueBarrierWithWaitListTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrierWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrierWithWaitList, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueBarrierWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueBarrierWithWaitList, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueBarrierWithWaitListTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueBarrierWithWaitList params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueCopyBufferTracer {
  public:
    clEnqueueCopyBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueCopyBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueCopyBufferRectTracer {
  public:
    clEnqueueCopyBufferRectTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferRect, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferRect, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueCopyBufferRectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyBufferRect params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueCopyBufferToImageTracer {
  public:
    clEnqueueCopyBufferToImageTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferToImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferToImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyBufferToImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyBufferToImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueCopyBufferToImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyBufferToImage params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueCopyImageTracer {
  public:
    clEnqueueCopyImageTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueCopyImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyImage params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueCopyImageToBufferTracer {
  public:
    clEnqueueCopyImageToBufferTracer() {}

    void enter(cl_command_queue *commandQueue,
               cl_mem *srcImage,
               cl_mem *dstBuffer,
               const size_t **srcOrigin,
               const size_t **region,
               size_t *dstOffset,
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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImageToBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImageToBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueCopyImageToBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueCopyImageToBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueCopyImageToBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueCopyImageToBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueFillBufferTracer {
  public:
    clEnqueueFillBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueFillBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueFillBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueFillImageTracer {
  public:
    clEnqueueFillImageTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueFillImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueFillImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueFillImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueFillImage params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueMapBufferTracer {
  public:
    clEnqueueMapBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueMapBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMapBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueMapImageTracer {
  public:
    clEnqueueMapImageTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMapImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMapImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueMapImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMapImage params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueMarkerTracer {
  public:
    clEnqueueMarkerTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarker)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarker, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarker)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarker, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueMarkerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMarker params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueMarkerWithWaitListTracer {
  public:
    clEnqueueMarkerWithWaitListTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarkerWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarkerWithWaitList, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMarkerWithWaitList)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMarkerWithWaitList, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueMarkerWithWaitListTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMarkerWithWaitList params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueMigrateMemObjectsTracer {
  public:
    clEnqueueMigrateMemObjectsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMigrateMemObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMigrateMemObjects, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueMigrateMemObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueMigrateMemObjects, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueMigrateMemObjectsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueMigrateMemObjects params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueNDRangeKernelTracer {
  public:
    clEnqueueNDRangeKernelTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNDRangeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNDRangeKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNDRangeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNDRangeKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueNDRangeKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueNDRangeKernel params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueNativeKernelTracer {
  public:
    clEnqueueNativeKernelTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNativeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNativeKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueNativeKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueNativeKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueNativeKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueNativeKernel params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueReadBufferTracer {
  public:
    clEnqueueReadBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueReadBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReadBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueReadBufferRectTracer {
  public:
    clEnqueueReadBufferRectTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBufferRect, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadBufferRect, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueReadBufferRectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReadBufferRect params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueReadImageTracer {
  public:
    clEnqueueReadImageTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReadImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReadImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueReadImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReadImage params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueSVMFreeTracer {
  public:
    clEnqueueSVMFreeTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMFree, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMFree, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueSVMFreeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMFree params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueSVMMapTracer {
  public:
    clEnqueueSVMMapTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMap, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMap, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueSVMMapTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMap params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueSVMMemFillTracer {
  public:
    clEnqueueSVMMemFillTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemFill)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemFill, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemFill)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemFill, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueSVMMemFillTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMemFill params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueSVMMemcpyTracer {
  public:
    clEnqueueSVMMemcpyTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemcpy)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemcpy, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMemcpy)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMemcpy, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueSVMMemcpyTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMemcpy params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueSVMMigrateMemTracer {
  public:
    clEnqueueSVMMigrateMemTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMigrateMem)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMigrateMem, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMMigrateMem)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMMigrateMem, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueSVMMigrateMemTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMMigrateMem params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueSVMUnmapTracer {
  public:
    clEnqueueSVMUnmapTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMUnmap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMUnmap, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueSVMUnmap)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueSVMUnmap, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueSVMUnmapTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueSVMUnmap params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueTaskTracer {
  public:
    clEnqueueTaskTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueTask)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueTask, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueTask)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueTask, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueTaskTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueTask params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueUnmapMemObjectTracer {
  public:
    clEnqueueUnmapMemObjectTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueUnmapMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueUnmapMemObject, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueUnmapMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueUnmapMemObject, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueUnmapMemObjectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueUnmapMemObject params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueWaitForEventsTracer {
  public:
    clEnqueueWaitForEventsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWaitForEvents, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWaitForEvents, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueWaitForEventsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWaitForEvents params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueWriteBufferTracer {
  public:
    clEnqueueWriteBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueWriteBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWriteBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueWriteBufferRectTracer {
  public:
    clEnqueueWriteBufferRectTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBufferRect, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteBufferRect)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteBufferRect, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueWriteBufferRectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWriteBufferRect params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueWriteImageTracer {
  public:
    clEnqueueWriteImageTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueWriteImage)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueWriteImage, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueWriteImageTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueWriteImage params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clFinishTracer {
  public:
    clFinishTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clFinish";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFinish)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFinish, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFinish)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFinish, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clFinishTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clFinish params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clFlushTracer {
  public:
    clFlushTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clFlush";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFlush)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFlush, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clFlush)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clFlush, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clFlushTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clFlush params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetCommandQueueInfoTracer {
  public:
    clGetCommandQueueInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetCommandQueueInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetCommandQueueInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetCommandQueueInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetCommandQueueInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetCommandQueueInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetCommandQueueInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetContextInfoTracer {
  public:
    clGetContextInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetContextInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetContextInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetContextInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetContextInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetContextInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetContextInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetDeviceAndHostTimerTracer {
  public:
    clGetDeviceAndHostTimerTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceAndHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceAndHostTimer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceAndHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceAndHostTimer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetDeviceAndHostTimerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceAndHostTimer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetDeviceIDsTracer {
  public:
    clGetDeviceIDsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceIDs, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceIDs, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetDeviceIDsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceIDs params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetDeviceInfoTracer {
  public:
    clGetDeviceInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetDeviceInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetDeviceInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetDeviceInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetDeviceInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetEventInfoTracer {
  public:
    clGetEventInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetEventInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetEventInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetEventProfilingInfoTracer {
  public:
    clGetEventProfilingInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventProfilingInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventProfilingInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetEventProfilingInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetEventProfilingInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetEventProfilingInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetEventProfilingInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetExtensionFunctionAddressTracer {
  public:
    clGetExtensionFunctionAddressTracer() {}

    void enter(const char **funcName) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.funcName = funcName;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clGetExtensionFunctionAddress";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddress)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddress, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddress)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddress, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetExtensionFunctionAddressTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetExtensionFunctionAddress params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetExtensionFunctionAddressForPlatformTracer {
  public:
    clGetExtensionFunctionAddressForPlatformTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetExtensionFunctionAddressForPlatform, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetExtensionFunctionAddressForPlatformTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetExtensionFunctionAddressForPlatform params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetHostTimerTracer {
  public:
    clGetHostTimerTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetHostTimer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetHostTimer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetHostTimer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetHostTimerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetHostTimer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetImageInfoTracer {
  public:
    clGetImageInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetImageInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetImageInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetImageInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetImageInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetImageInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetImageInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetKernelArgInfoTracer {
  public:
    clGetKernelArgInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelArgInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelArgInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelArgInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelArgInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetKernelArgInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelArgInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetKernelInfoTracer {
  public:
    clGetKernelInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetKernelInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetKernelSubGroupInfoTracer {
  public:
    clGetKernelSubGroupInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSubGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSubGroupInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelSubGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelSubGroupInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetKernelSubGroupInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelSubGroupInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetKernelWorkGroupInfoTracer {
  public:
    clGetKernelWorkGroupInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelWorkGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelWorkGroupInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetKernelWorkGroupInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetKernelWorkGroupInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetKernelWorkGroupInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetKernelWorkGroupInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetMemObjectInfoTracer {
  public:
    clGetMemObjectInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetMemObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetMemObjectInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetMemObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetMemObjectInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetMemObjectInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetMemObjectInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetPipeInfoTracer {
  public:
    clGetPipeInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPipeInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPipeInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPipeInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPipeInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetPipeInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetPipeInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetPlatformIDsTracer {
  public:
    clGetPlatformIDsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformIDs, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformIDs)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformIDs, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetPlatformIDsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetPlatformIDs params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetPlatformInfoTracer {
  public:
    clGetPlatformInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetPlatformInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetPlatformInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetPlatformInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetPlatformInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetProgramBuildInfoTracer {
  public:
    clGetProgramBuildInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramBuildInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramBuildInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramBuildInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramBuildInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetProgramBuildInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetProgramBuildInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetProgramInfoTracer {
  public:
    clGetProgramInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetProgramInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetProgramInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetProgramInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetProgramInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetSamplerInfoTracer {
  public:
    clGetSamplerInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSamplerInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSamplerInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSamplerInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSamplerInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetSamplerInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetSamplerInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetSupportedImageFormatsTracer {
  public:
    clGetSupportedImageFormatsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSupportedImageFormats)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSupportedImageFormats, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetSupportedImageFormats)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetSupportedImageFormats, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetSupportedImageFormatsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetSupportedImageFormats params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clLinkProgramTracer {
  public:
    clLinkProgramTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clLinkProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clLinkProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_program *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clLinkProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clLinkProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clLinkProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clLinkProgram params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseCommandQueueTracer {
  public:
    clReleaseCommandQueueTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseCommandQueue";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseCommandQueue params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseContextTracer {
  public:
    clReleaseContextTracer() {}

    void enter(cl_context *context) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseContext";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseContext, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseContext, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseContextTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseContext params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseDeviceTracer {
  public:
    clReleaseDeviceTracer() {}

    void enter(cl_device_id *device) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseDevice";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseDevice, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseDevice, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseDeviceTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseDevice params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseEventTracer {
  public:
    clReleaseEventTracer() {}

    void enter(cl_event *event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseEvent";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseEvent, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseEvent, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseEventTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseEvent params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseKernelTracer {
  public:
    clReleaseKernelTracer() {}

    void enter(cl_kernel *kernel) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseKernel params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseMemObjectTracer {
  public:
    clReleaseMemObjectTracer() {}

    void enter(cl_mem *memobj) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseMemObject";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseMemObject, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseMemObject, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseMemObjectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseMemObject params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseProgramTracer {
  public:
    clReleaseProgramTracer() {}

    void enter(cl_program *program) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseProgram params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clReleaseSamplerTracer {
  public:
    clReleaseSamplerTracer() {}

    void enter(cl_sampler *sampler) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.sampler = sampler;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clReleaseSampler";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseSampler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clReleaseSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clReleaseSampler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clReleaseSamplerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clReleaseSampler params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainCommandQueueTracer {
  public:
    clRetainCommandQueueTracer() {}

    void enter(cl_command_queue *commandQueue) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.commandQueue = commandQueue;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainCommandQueue";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainCommandQueue params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainContextTracer {
  public:
    clRetainContextTracer() {}

    void enter(cl_context *context) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.context = context;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainContext";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainContext, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainContext)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainContext, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainContextTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainContext params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainDeviceTracer {
  public:
    clRetainDeviceTracer() {}

    void enter(cl_device_id *device) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.device = device;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainDevice";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainDevice, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainDevice)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainDevice, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainDeviceTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainDevice params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainEventTracer {
  public:
    clRetainEventTracer() {}

    void enter(cl_event *event) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.event = event;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainEvent";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainEvent, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainEvent)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainEvent, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainEventTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainEvent params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainKernelTracer {
  public:
    clRetainKernelTracer() {}

    void enter(cl_kernel *kernel) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.kernel = kernel;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainKernel";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainKernel)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainKernel, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainKernelTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainKernel params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainMemObjectTracer {
  public:
    clRetainMemObjectTracer() {}

    void enter(cl_mem *memobj) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.memobj = memobj;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainMemObject";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainMemObject, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainMemObject)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainMemObject, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainMemObjectTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainMemObject params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainProgramTracer {
  public:
    clRetainProgramTracer() {}

    void enter(cl_program *program) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.program = program;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainProgram";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainProgram)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainProgram, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainProgramTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainProgram params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clRetainSamplerTracer {
  public:
    clRetainSamplerTracer() {}

    void enter(cl_sampler *sampler) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.sampler = sampler;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clRetainSampler";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainSampler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clRetainSampler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clRetainSampler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clRetainSamplerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clRetainSampler params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSVMAllocTracer {
  public:
    clSVMAllocTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMAlloc)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMAlloc, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void **retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMAlloc)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMAlloc, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSVMAllocTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSVMAlloc params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSVMFreeTracer {
  public:
    clSVMFreeTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMFree, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(void *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSVMFree)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSVMFree, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSVMFreeTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSVMFree params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetCommandQueuePropertyTracer {
  public:
    clSetCommandQueuePropertyTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetCommandQueueProperty)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetCommandQueueProperty, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetCommandQueueProperty)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetCommandQueueProperty, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetCommandQueuePropertyTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetCommandQueueProperty params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetDefaultDeviceCommandQueueTracer {
  public:
    clSetDefaultDeviceCommandQueueTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetDefaultDeviceCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetDefaultDeviceCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetDefaultDeviceCommandQueue)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetDefaultDeviceCommandQueue, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetDefaultDeviceCommandQueueTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetDefaultDeviceCommandQueue params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetEventCallbackTracer {
  public:
    clSetEventCallbackTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetEventCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetEventCallback, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetEventCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetEventCallback, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetEventCallbackTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetEventCallback params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetKernelArgTracer {
  public:
    clSetKernelArgTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArg)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArg, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArg)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArg, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetKernelArgTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetKernelArg params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetKernelArgSVMPointerTracer {
  public:
    clSetKernelArgSVMPointerTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArgSVMPointer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArgSVMPointer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelArgSVMPointer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelArgSVMPointer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetKernelArgSVMPointerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetKernelArgSVMPointer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetKernelExecInfoTracer {
  public:
    clSetKernelExecInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelExecInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelExecInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetKernelExecInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetKernelExecInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetKernelExecInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetKernelExecInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetMemObjectDestructorCallbackTracer {
  public:
    clSetMemObjectDestructorCallbackTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetMemObjectDestructorCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetMemObjectDestructorCallback, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetMemObjectDestructorCallback)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetMemObjectDestructorCallback, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetMemObjectDestructorCallbackTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetMemObjectDestructorCallback params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clSetUserEventStatusTracer {
  public:
    clSetUserEventStatusTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetUserEventStatus)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetUserEventStatus, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clSetUserEventStatus)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clSetUserEventStatus, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clSetUserEventStatusTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clSetUserEventStatus params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clUnloadCompilerTracer {
  public:
    clUnloadCompilerTracer() {}

    void enter() {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clUnloadCompiler";
        data.functionParams = nullptr;
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadCompiler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadCompiler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clUnloadCompilerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clUnloadPlatformCompilerTracer {
  public:
    clUnloadPlatformCompilerTracer() {}

    void enter(cl_platform_id *platform) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_NOTHING_CALLED);

        params.platform = platform;

        data.site = CL_CALLBACK_SITE_ENTER;
        data.correlationId = tracingCorrelationId.fetch_add(1, std::memory_order_acq_rel);
        data.functionName = "clUnloadPlatformCompiler";
        data.functionParams = static_cast<const void *>(&params);
        data.functionReturnValue = nullptr;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadPlatformCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadPlatformCompiler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clUnloadPlatformCompiler)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clUnloadPlatformCompiler, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clUnloadPlatformCompilerTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clUnloadPlatformCompiler params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clWaitForEventsTracer {
  public:
    clWaitForEventsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clWaitForEvents, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clWaitForEvents)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clWaitForEvents, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clWaitForEventsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clWaitForEvents params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

#ifdef _WIN32

class clCreateFromGLBufferTracer {
  public:
    clCreateFromGLBufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLBuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLBuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateFromGLBufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLBuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateFromGLRenderbufferTracer {
  public:
    clCreateFromGLRenderbufferTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLRenderbuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLRenderbuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLRenderbuffer)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLRenderbuffer, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateFromGLRenderbufferTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLRenderbuffer params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateFromGLTextureTracer {
  public:
    clCreateFromGLTextureTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateFromGLTextureTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLTexture params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateFromGLTexture2DTracer {
  public:
    clCreateFromGLTexture2DTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture2D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture2D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture2D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateFromGLTexture2DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLTexture2D params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clCreateFromGLTexture3DTracer {
  public:
    clCreateFromGLTexture3DTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture3D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_mem *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clCreateFromGLTexture3D)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clCreateFromGLTexture3D, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clCreateFromGLTexture3DTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clCreateFromGLTexture3D params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueAcquireGLObjectsTracer {
  public:
    clEnqueueAcquireGLObjectsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueAcquireGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueAcquireGLObjects, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueAcquireGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueAcquireGLObjects, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueAcquireGLObjectsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueAcquireGLObjects params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clEnqueueReleaseGLObjectsTracer {
  public:
    clEnqueueReleaseGLObjectsTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReleaseGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReleaseGLObjects, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clEnqueueReleaseGLObjects)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clEnqueueReleaseGLObjects, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clEnqueueReleaseGLObjectsTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clEnqueueReleaseGLObjects params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetGLObjectInfoTracer {
  public:
    clGetGLObjectInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLObjectInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLObjectInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLObjectInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetGLObjectInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetGLObjectInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

class clGetGLTextureInfoTracer {
  public:
    clGetGLTextureInfoTracer() {}

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

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLTextureInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLTextureInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_ENTER_CALLED;
    }

    void exit(cl_int *retVal) {
        DEBUG_BREAK_IF(state != TRACING_NOTIFY_STATE_ENTER_CALLED);
        data.site = CL_CALLBACK_SITE_EXIT;
        data.functionReturnValue = retVal;

        DEBUG_BREAK_IF(tracingHandle.size() == 0);
        DEBUG_BREAK_IF(tracingHandle.size() >= TRACING_MAX_HANDLE_COUNT);
        for (size_t i = 0; i < tracingHandle.size(); ++i) {
            TracingHandle *handle = tracingHandle[i];
            DEBUG_BREAK_IF(handle == nullptr);
            if (handle->getTracingPoint(CL_FUNCTION_clGetGLTextureInfo)) {
                data.correlationData = correlationData + i;
                handle->call(CL_FUNCTION_clGetGLTextureInfo, &data);
            }
        }

        state = TRACING_NOTIFY_STATE_EXIT_CALLED;
    }

    ~clGetGLTextureInfoTracer() {
        DEBUG_BREAK_IF(state == TRACING_NOTIFY_STATE_ENTER_CALLED);
    }

  private:
    cl_params_clGetGLTextureInfo params;
    cl_callback_data data;
    uint64_t correlationData[TRACING_MAX_HANDLE_COUNT];
    tracing_notify_state_t state = TRACING_NOTIFY_STATE_NOTHING_CALLED;
};

#endif

} // namespace HostSideTracing
