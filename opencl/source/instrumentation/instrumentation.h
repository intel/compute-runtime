/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
extern const bool haveInstrumentation;
} // namespace NEO

namespace MetricsLibraryApi {
// Dummy macros.
#define ML_STDCALL
#define METRICS_LIBRARY_CONTEXT_CREATE_1_0 "create"
#define METRICS_LIBRARY_CONTEXT_DELETE_1_0 "delete"

// Dummy enumerators.
enum class ClientApi : uint32_t { OpenCL };
enum class ClientGen : uint32_t { Unknown,
                                  Gen9,
                                  Gen11,
                                  Gen12 };
enum class ValueType : uint32_t { Uint32 };
enum class GpuConfigurationActivationType : uint32_t { Tbs,
                                                       EscapeCode };
enum class ObjectType : uint32_t { QueryHwCounters,
                                   ConfigurationHwCountersUser,
                                   ConfigurationHwCountersOa };
enum class ParameterType : uint32_t { QueryHwCountersReportApiSize,
                                      QueryHwCountersReportGpuSize };
enum class StatusCode : uint32_t { Failed,
                                   IncorrectObject,
                                   Success };
enum class GpuCommandBufferType : uint32_t { Render,
                                             Compute };

enum class ClientOptionsType : uint32_t {
    Compute
};

// Dummy handles.
struct Handle {
    void *data;
    bool IsValid() const { return data != nullptr; } // NOLINT
};
struct QueryHandle_1_0 : Handle {};
struct ConfigurationHandle_1_0 : Handle {};
struct ContextHandle_1_0 : Handle {};

// Dummy structures.
struct ClientCallbacks_1_0 {};

struct ClientDataWindows_1_0 {
    void *Device;
    void *Adapter;
    void *Escape;
    bool KmdInstrumentationEnabled;
};

struct ClientDataLinux_1_0 {
    void *Reserved;
};

struct ClientOptionsComputeData_1_0 {
    bool Asynchronous;
};

struct ClientOptionsData_1_0 {
    ClientOptionsType Type;
    ClientOptionsComputeData_1_0 Compute;
};

struct ClientData_1_0 {
    union {
        ClientDataWindows_1_0 Windows;
        ClientDataLinux_1_0 Linux;
    };
    ClientOptionsData_1_0 *ClientOptions;
    uint32_t ClientOptionsCount;
};

struct ConfigurationActivateData_1_0 {
    GpuConfigurationActivationType Type;
};

struct ClientType_1_0 {
    ClientApi Api;
    ClientGen Gen;
};

struct TypedValue_1_0 {
    uint32_t ValueUInt32;
};

struct GpuMemory_1_0 {
    uint64_t GpuAddress;
    void *CpuAddress;
};

struct CommandBufferQueryHwCounters_1_0 {
    QueryHandle_1_0 Handle;
    ConfigurationHandle_1_0 HandleUserConfiguration;
    bool Begin;
};

struct CommandBufferSize_1_0 {
    uint32_t GpuMemorySize;
};

struct ConfigurationCreateData_1_0 {
    ContextHandle_1_0 HandleContext;
    ObjectType Type;
};

struct CommandBufferData_1_0 {
    ContextHandle_1_0 HandleContext;
    ObjectType CommandsType;
    GpuCommandBufferType Type;
    GpuMemory_1_0 Allocation;
    void *Data;
    uint32_t Size;
    CommandBufferQueryHwCounters_1_0 QueryHwCounters;
};

struct QueryCreateData_1_0 {
    ContextHandle_1_0 HandleContext;
    ObjectType Type;
    uint32_t Slots;
};

struct GetReportQuery_1_0 {
    QueryHandle_1_0 Handle;

    uint32_t Slot;
    uint32_t SlotsCount;

    uint32_t DataSize;
    void *Data;
};

struct GetReportData_1_0 {
    ObjectType Type;
    GetReportQuery_1_0 Query;
};

struct ContextCreateData_1_0 {
    ClientData_1_0 *ClientData;
    ClientCallbacks_1_0 *ClientCallbacks;
    struct Interface_1_0 *Api;
};

// Dummy functions.
using ContextCreateFunction_1_0 = StatusCode(ML_STDCALL *)(ClientType_1_0 clientType, struct ContextCreateData_1_0 *createData, ContextHandle_1_0 *handle);
using ContextDeleteFunction_1_0 = StatusCode(ML_STDCALL *)(const ContextHandle_1_0 handle);
using GetParameterFunction_1_0 = StatusCode(ML_STDCALL *)(const ParameterType parameter, ValueType *type, TypedValue_1_0 *value);
using CommandBufferGetFunction_1_0 = StatusCode(ML_STDCALL *)(const CommandBufferData_1_0 *data);
using CommandBufferGetSizeFunction_1_0 = StatusCode(ML_STDCALL *)(const CommandBufferData_1_0 *data, CommandBufferSize_1_0 *size);
using QueryCreateFunction_1_0 = StatusCode(ML_STDCALL *)(const QueryCreateData_1_0 *createData, QueryHandle_1_0 *handle);
using QueryDeleteFunction_1_0 = StatusCode(ML_STDCALL *)(const QueryHandle_1_0 handle);
using ConfigurationCreateFunction_1_0 = StatusCode(ML_STDCALL *)(const ConfigurationCreateData_1_0 *createData, ConfigurationHandle_1_0 *handle);
using ConfigurationActivateFunction_1_0 = StatusCode(ML_STDCALL *)(const ConfigurationHandle_1_0 handle, const ConfigurationActivateData_1_0 *activateData);
using ConfigurationDeactivateFunction_1_0 = StatusCode(ML_STDCALL *)(const ConfigurationHandle_1_0 handle);
using ConfigurationDeleteFunction_1_0 = StatusCode(ML_STDCALL *)(const ConfigurationHandle_1_0 handle);
using GetDataFunction_1_0 = StatusCode(ML_STDCALL *)(GetReportData_1_0 *data);

// Dummy interface.
struct Interface_1_0 {
    GetParameterFunction_1_0 GetParameter;

    CommandBufferGetFunction_1_0 CommandBufferGet;
    CommandBufferGetSizeFunction_1_0 CommandBufferGetSize;

    QueryCreateFunction_1_0 QueryCreate;
    QueryDeleteFunction_1_0 QueryDelete;

    ConfigurationCreateFunction_1_0 ConfigurationCreate;
    ConfigurationActivateFunction_1_0 ConfigurationActivate;
    ConfigurationDeactivateFunction_1_0 ConfigurationDeactivate;
    ConfigurationDeleteFunction_1_0 ConfigurationDelete;

    GetDataFunction_1_0 GetData;
};
}; // namespace MetricsLibraryApi