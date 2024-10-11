/*========================== begin_copyright_notice ============================

Copyright (C) 2020-2024 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

/*
@file metrics_library_api_1_0.h

@brief Interface for Metrics Library umd dll.
*/

#pragma once

#include <stdint.h>

#ifdef _MSC_VER
    #define ML_STDCALL __stdcall
#else
    #define ML_STDCALL
#endif

namespace MetricsLibraryApi
{
//////////////////////////////////////////////////////////////////////////
/// @brief Metrics Library handles.
//////////////////////////////////////////////////////////////////////////
#define METRICS_LIBRARY_HANDLE( Object ) typedef struct Object##Handle_1_0                                   \
                                         {                                                                   \
                                             void* data;                                                     \
                                             Object##Handle_1_0( void )          { data = nullptr; }         \
                                             Object##Handle_1_0( void* handle )  { data = handle; }          \
                                             bool IsValid( void ) const          { return data != nullptr; } \
                                         } Object##Handle_1_0;

METRICS_LIBRARY_HANDLE( Client )
METRICS_LIBRARY_HANDLE( ClientMemory )
METRICS_LIBRARY_HANDLE( Configuration )
METRICS_LIBRARY_HANDLE( Context )
METRICS_LIBRARY_HANDLE( Marker )
METRICS_LIBRARY_HANDLE( Override )
METRICS_LIBRARY_HANDLE( Query )

//////////////////////////////////////////////////////////////////////////
/// @brief Client api types.
//////////////////////////////////////////////////////////////////////////
enum class ClientApi : uint32_t
{
    Unknown = 0,
    // 1-5 reserved.
    OpenCL = 6,
    // 7-8 reserved.
    OneApi = 9,
    // 10 reserved.
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client gpu generation types.
//////////////////////////////////////////////////////////////////////////
enum class ClientGen : uint32_t
{
    Unknown = 0,
    Gen9    = 1,
    Gen9LP  = 2,
    Gen11   = 4,
    Gen11LP = 5,
    Gen12   = 6,
    XeHP    = 9,
    XeHPG   = 10,
    XeHPC   = 11,
    Xe2HPG  = 12,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Library object types.
//////////////////////////////////////////////////////////////////////////
enum class ObjectType : uint32_t
{
    Unknown                         = 0,

    // Context objects:
    Context                         = 10000,

    // Query objects:
    QueryHwCounters                 = 20000,
    QueryPipelineTimestamps         = 20001,
    QueryHwCountersCopyReports      = 20002,

    // Configuration objects:
    ConfigurationHwCountersOa       = 30000,
    ConfigurationHwCountersUser,

    // Override objects:
    OverrideUser                    = 40000,
    OverrideNullHardware,
    OverrideFlushCaches,
    OverridePoshQuery,
    OverrideDisablePoshPrimitives,

    // Markers:
    MarkerStreamUser                = 50000,
    MarkerStreamUserExtended        = 50001,

    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options types.
//////////////////////////////////////////////////////////////////////////
enum class ClientOptionsType : uint32_t
{
    Posh = 0,
    Ptbr,
    Compute,
    Tbs,
    SubDevice,
    SubDeviceIndex,
    SubDeviceCount,
    WorkloadPartition,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Status code types.
//////////////////////////////////////////////////////////////////////////
enum class StatusCode : uint32_t
{
    Success = 0,
    Failed,
    IncorrectVersion,
    IncorrectParameter,
    IncorrectSlot,
    IncorrectObject,
    InsufficientSpace,
    NotInitialized,
    NotSupported,
    NotImplemented,
    NullPointer,
    OutOfMemory,
    UnknownGen,
    TbsUnableToEnable,
    TbsUnableToRead,
    ReportNotReady,
    ReportLost,
    ReportInconsistent,
    CannotOpenFile,
    ReportContextSwitchLost,
    ReportWithoutWorkload,
    ContextMismatch,
    ReportWithEmptyConfiguration,
    ReportQueryModeMismatch,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Value types.
//////////////////////////////////////////////////////////////////////////
enum class ValueType : uint8_t
{
    Uint8 = 0,
    Int8,
    Uint16,
    Int16,
    Uint32,
    Int32,
    Uint64,
    Int64,
    Float,
    Double,
    Bool,
    String,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Gpu configuration activation types.
//////////////////////////////////////////////////////////////////////////
enum class GpuConfigurationActivationType : uint32_t
{
    EscapeCode = 0,
    Tbs,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Gpu command buffer types.
//////////////////////////////////////////////////////////////////////////
enum class GpuCommandBufferType : uint32_t
{
    Render = 0,
    Posh,
    Tile,
    Compute,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Memory types.
//////////////////////////////////////////////////////////////////////////
enum class MemoryType : uint32_t
{
    Query = 0,
    CommandBuffer,
    Cpu,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Memory flags.
//////////////////////////////////////////////////////////////////////////
enum class MemoryFlags : uint32_t
{
    Linear      = 1 << 0,
    Cachable    = 1 << 1
};

//////////////////////////////////////////////////////////////////////////
/// @brief Parameter types.
//////////////////////////////////////////////////////////////////////////
enum class ParameterType : uint32_t
{
    QueryHwCountersReportApiSize = 0,
    QueryHwCountersReportGpuSize,
    QueryPipelineTimestampsReportApiSize,
    QueryPipelineTimestampsReportGpuSize,
    LibraryBuildNumber,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Linux adapter types.
//////////////////////////////////////////////////////////////////////////
enum class LinuxAdapterType : uint8_t
{
    DrmFileDescriptor = 0,
    // ...
    Last
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client type.
//////////////////////////////////////////////////////////////////////////
struct ClientType_1_0
{
    ClientApi    Api;
    ClientGen    Gen;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Typed value.
//////////////////////////////////////////////////////////////////////////
struct TypedValue_1_0
{
    ValueType    Type;

    union
    {
        int8_t         ValueInt8;
        uint8_t        ValueUInt8;
        int16_t        ValueInt16;
        uint16_t       ValueUInt16;
        int32_t        ValueInt32;
        uint32_t       ValueUInt32;
        int64_t        ValueInt64;
        uint64_t       ValueUInt64;
        float          ValueFloat;
        bool           ValueBool;
        const char*    ValueString;
    };
};

//////////////////////////////////////////////////////////////////////////
/// @brief Gpu memory data.
//////////////////////////////////////////////////////////////////////////
struct GpuMemory_1_0
{
    uint64_t                  GpuAddress;      // Virtual gpu address.
    void*                     CpuAddress;      // Locked cpu address.
    ClientMemoryHandle_1_0    HandleMemory;    // Driver's handle.
};

//////////////////////////////////////////////////////////////////////////
/// @brief Query creation data.
//////////////////////////////////////////////////////////////////////////
struct QueryCreateData_1_0
{
    ContextHandle_1_0    HandleContext;
    ObjectType           Type;
    uint32_t             Slots;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Query hw counters command buffer data.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferQueryHwCounters_1_0
{
    QueryHandle_1_0            Handle;
    ConfigurationHandle_1_0    HandleUserConfiguration;
    uint32_t                   Slot;
    uint64_t                   MarkerUser;
    uint64_t                   MarkerDriver;
    uint64_t                   EndTag;
    bool                       Begin;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Copy query hw counter reports command buffer data.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferQueryHwCountersCopyReports_1_0
{
    QueryHandle_1_0    HandleSource;
    QueryHandle_1_0    HandleTarget;

    GpuMemory_1_0      AddressSource;
    GpuMemory_1_0      AddressTarget;

    uint32_t           SlotSource;
    uint32_t           SlotTarget;

    uint32_t           SlotCount;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Query pipeline timestamps command buffer data.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferQueryPipelineTimestamps_1_0
{
    QueryHandle_1_0    Handle;
    uint64_t           EndTag;
    bool               Begin;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Overrides commands data.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferOverride_1_0
{
    OverrideHandle_1_0    Handle;
    bool                  Enable;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Marker stream user commands data.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferMarkerStreamUser_1_0
{
    uint32_t    Value;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Marker stream user extended commands data.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferMarkerStreamUserExtended_1_0
{
};

//////////////////////////////////////////////////////////////////////////
/// @brief Command buffer data.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferData_1_0
{
    ContextHandle_1_0       HandleContext;    // Context handle.
    ObjectType              CommandsType;     // Commands type related to library object.
    GpuCommandBufferType    Type;             // Gpu command buffer type.
    GpuMemory_1_0           Allocation;       // Gpu memory allocation.
    void*                   Data;             // Gpu command buffer data.
    uint32_t                Size;             // Gpu command buffer size.
    uint32_t                Offset;           // Gpu command buffer offset from beginning.

    union
    {
        CommandBufferQueryHwCounters_1_0               QueryHwCounters;
        CommandBufferQueryHwCountersCopyReports_1_0    QueryHwCountersCopyReports;
        CommandBufferQueryPipelineTimestamps_1_0       QueryPipelineTimestamps;
        CommandBufferOverride_1_0                      Override;
        CommandBufferMarkerStreamUser_1_0              MarkerStreamUser;
        CommandBufferMarkerStreamUserExtended_1_0      MarkerStreamUserExtended;
    };
};

//////////////////////////////////////////////////////////////////////////
/// @brief Command buffer size.
//////////////////////////////////////////////////////////////////////////
struct CommandBufferSize_1_0
{
    uint32_t    GpuMemorySize;
    uint32_t    GpuMemoryPatchesCount;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Override create data.
//////////////////////////////////////////////////////////////////////////
struct OverrideCreateData_1_0
{
    ContextHandle_1_0    HandleContext;
    ObjectType           Type;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Marker creation data.
//////////////////////////////////////////////////////////////////////////
struct MarkerCreateData_1_0
{
    ContextHandle_1_0    HandleContext;
    ObjectType           Type;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Configuration creation data.
//////////////////////////////////////////////////////////////////////////
struct ConfigurationCreateData_1_0
{
    ContextHandle_1_0    HandleContext;
    ObjectType           Type;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Configuration activate data.
//////////////////////////////////////////////////////////////////////////
struct ConfigurationActivateData_1_0
{
    GpuConfigurationActivationType    Type;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Get query report.
//////////////////////////////////////////////////////////////////////////
struct GetReportQuery_1_0
{
    QueryHandle_1_0    Handle;        // Input.

    uint32_t           Slot;          // Input.
    uint32_t           SlotsCount;    // Input.

    uint32_t           DataSize;      // Input.
    void*              Data;          // Input / output.
};

//////////////////////////////////////////////////////////////////////////
/// @brief Get override report.
//////////////////////////////////////////////////////////////////////////
struct GetReportOverride_1_0
{
    OverrideHandle_1_0    Handle;      // Input.

    uint32_t              DataSize;    // Input.
    void*                 Data;        // Output.
};

//////////////////////////////////////////////////////////////////////////
/// @brief Get data.
//////////////////////////////////////////////////////////////////////////
struct GetReportData_1_0
{
    ObjectType    Type;

    union
    {
        GetReportQuery_1_0       Query;
        GetReportOverride_1_0    Override;
    };
};

//////////////////////////////////////////////////////////////////////////
/// @brief Gpu memory allocate data.
//////////////////////////////////////////////////////////////////////////
struct GpuMemoryAllocateData_1_0
{
    MemoryType     Type;     // Input.
    MemoryFlags    Flags;    // Input.
    uint32_t       Size;     // Input.
    void*          Data;     // Output.
};

//////////////////////////////////////////////////////////////////////////
/// @brief Cpu memory allocate data.
//////////////////////////////////////////////////////////////////////////
struct CpuMemoryAllocateData_1_0
{
    uint32_t    Size;    // Input.
    void*       Data;    // Output.
};

//////////////////////////////////////////////////////////////////////////
/// @brief Metrics Library functions.
//////////////////////////////////////////////////////////////////////////
using GetParameterFunction_1_0            = StatusCode( ML_STDCALL* ) ( const ParameterType parameter, ValueType* type, TypedValue_1_0* value );

using CommandBufferGetFunction_1_0        = StatusCode( ML_STDCALL* ) ( const CommandBufferData_1_0* data );
using CommandBufferGetSizeFunction_1_0    = StatusCode( ML_STDCALL* ) ( const CommandBufferData_1_0* data, CommandBufferSize_1_0* size );

using QueryCreateFunction_1_0             = StatusCode( ML_STDCALL* ) ( const QueryCreateData_1_0* createData, QueryHandle_1_0* handle );
using QueryDeleteFunction_1_0             = StatusCode( ML_STDCALL* ) ( const QueryHandle_1_0 handle );

using OverrideCreateFunction_1_0          = StatusCode( ML_STDCALL* ) ( const OverrideCreateData_1_0* createData, OverrideHandle_1_0* handle );
using OverrideDeleteFunction_1_0          = StatusCode( ML_STDCALL* ) ( const OverrideHandle_1_0 handle );

using MarkerCreateFunction_1_0            = StatusCode( ML_STDCALL* ) ( const MarkerCreateData_1_0* createData, MarkerHandle_1_0* handle );
using MarkerDeleteFunction_1_0            = StatusCode( ML_STDCALL* ) ( const MarkerHandle_1_0 handle );

using ConfigurationCreateFunction_1_0     = StatusCode( ML_STDCALL* ) ( const ConfigurationCreateData_1_0* createData, ConfigurationHandle_1_0* handle );
using ConfigurationActivateFunction_1_0   = StatusCode( ML_STDCALL* ) ( const ConfigurationHandle_1_0 handle, const ConfigurationActivateData_1_0* activateData );
using ConfigurationDeactivateFunction_1_0 = StatusCode( ML_STDCALL* ) ( const ConfigurationHandle_1_0 handle );
using ConfigurationDeleteFunction_1_0     = StatusCode( ML_STDCALL* ) ( const ConfigurationHandle_1_0 handle );

using GetDataFunction_1_0                 = StatusCode( ML_STDCALL* ) ( GetReportData_1_0* data );

//////////////////////////////////////////////////////////////////////////
/// @brief Metrics Library callbacks.
//////////////////////////////////////////////////////////////////////////
using CommandBufferFlushCallback_1_0    = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle );
using CommandBufferHasSpaceCallback_1_0 = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle );
using CommandBufferGetSpaceCallback_1_0 = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle );

using GpuMemoryAllocateCallback_1_0     = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle, GpuMemoryAllocateData_1_0* allocationData, ClientMemoryHandle_1_0* handleMemory );
using GpuMemoryPatchCallback_1_0        = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle, const ClientMemoryHandle_1_0 handleMemory, const uint64_t allocationOffset, const uint64_t buffer );
using GpuMemoryReleaseCallback_1_0      = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle );

using CpuMemoryAllocateCallback_1_0     = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle, CpuMemoryAllocateData_1_0* allocationData, ClientMemoryHandle_1_0* handleMemory );
using CpuMemoryReleaseCallback_1_0      = StatusCode ( ML_STDCALL* ) ( const ClientHandle_1_0 handle );

//////////////////////////////////////////////////////////////////////////
/// @brief Metrics Library interface.
//////////////////////////////////////////////////////////////////////////
struct Interface_1_0
{
    GetParameterFunction_1_0               GetParameter;

    CommandBufferGetFunction_1_0           CommandBufferGet;
    CommandBufferGetSizeFunction_1_0       CommandBufferGetSize;

    QueryCreateFunction_1_0                QueryCreate;
    QueryDeleteFunction_1_0                QueryDelete;

    OverrideCreateFunction_1_0             OverrideCreate;
    OverrideDeleteFunction_1_0             OverrideDelete;

    MarkerCreateFunction_1_0               MarkerCreate;
    MarkerDeleteFunction_1_0               MarkerDelete;

    ConfigurationCreateFunction_1_0        ConfigurationCreate;
    ConfigurationActivateFunction_1_0      ConfigurationActivate;
    ConfigurationDeactivateFunction_1_0    ConfigurationDeactivate;
    ConfigurationDeleteFunction_1_0        ConfigurationDelete;

    GetDataFunction_1_0                    GetData;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Metrics Library interface callbacks.
//////////////////////////////////////////////////////////////////////////
struct ClientCallbacks_1_0
{
    CommandBufferFlushCallback_1_0       CommandBufferFlush;
    CommandBufferHasSpaceCallback_1_0    CommandBufferHasSpace;
    CommandBufferGetSpaceCallback_1_0    CommandBufferGetSpace;

    GpuMemoryAllocateCallback_1_0        GpuMemoryAllocate;
    GpuMemoryPatchCallback_1_0           GpuMemoryPatch;
    GpuMemoryReleaseCallback_1_0         GpuMemoryRelease;

    CpuMemoryAllocateCallback_1_0        CpuMemoryAllocate;
    CpuMemoryReleaseCallback_1_0         CpuMemoryRelease;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Linux client adapter data.
//////////////////////////////////////////////////////////////////////////
struct ClientDataLinuxAdapter_1_0
{
    LinuxAdapterType    Type;

    union
    {
        int32_t    DrmFileDescriptor;
    };
};

//////////////////////////////////////////////////////////////////////////
/// @brief Windows client data.
//////////////////////////////////////////////////////////////////////////
struct ClientDataWindows_1_0
{
    void*    Device;
    void*    Adapter;
    void*    Escape;
    bool     KmdInstrumentationEnabled;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Linux client data.
//////////////////////////////////////////////////////////////////////////
struct ClientDataLinux_1_0
{
    union
    {
        ClientDataLinuxAdapter_1_0*    Adapter;
        void*                          Reserved;
    };
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options posh data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsPoshData_1_0
{
    bool    Enabled;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options ptbr data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsPtbrData_1_0
{
    bool    Enabled;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options compute data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsComputeData_1_0
{
    bool    Asynchronous;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options time based sampling data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsTbsData_1_0
{
    bool    Enabled;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options sub device data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsSubDeviceData_1_0
{
    bool    Enabled;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options sub device index data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsSubDeviceIndexData_1_0
{
    uint8_t    Index;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options sub device count data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsSubDeviceCountData_1_0
{
    uint8_t    Count;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options workload partition data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsWorkloadPartition_1_0
{
    bool    Enabled;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client options data.
//////////////////////////////////////////////////////////////////////////
struct ClientOptionsData_1_0
{
    ClientOptionsType    Type;

    union
    {
        ClientOptionsPoshData_1_0              Posh;
        ClientOptionsPtbrData_1_0              Ptbr;
        ClientOptionsComputeData_1_0           Compute;
        ClientOptionsTbsData_1_0               Tbs;
        ClientOptionsSubDeviceData_1_0         SubDevice;
        ClientOptionsSubDeviceIndexData_1_0    SubDeviceIndex;
        ClientOptionsSubDeviceCountData_1_0    SubDeviceCount;
        ClientOptionsWorkloadPartition_1_0     WorkloadPartition;
    };
};

//////////////////////////////////////////////////////////////////////////
/// @brief Client data.
//////////////////////////////////////////////////////////////////////////
struct ClientData_1_0
{
    ClientHandle_1_0    Handle;

    union
    {
        ClientDataWindows_1_0    Windows;
        ClientDataLinux_1_0      Linux;
    };

    ClientOptionsData_1_0*    ClientOptions;
    uint32_t                  ClientOptionsCount;
};

//////////////////////////////////////////////////////////////////////////
/// @brief Context create data.
//////////////////////////////////////////////////////////////////////////
struct ContextCreateData_1_0
{
    ClientData_1_0*        ClientData;
    ClientCallbacks_1_0*   ClientCallbacks;
    Interface_1_0*         Api;
};

using ContextCreateFunction_1_0 = StatusCode ( ML_STDCALL* ) ( ClientType_1_0 clientType, ContextCreateData_1_0* createData, ContextHandle_1_0* handle );
using ContextDeleteFunction_1_0 = StatusCode ( ML_STDCALL* ) ( const ContextHandle_1_0 handle );

//////////////////////////////////////////////////////////////////////////
/// @brief Metrics Library initialize/destroy functions.
//////////////////////////////////////////////////////////////////////////
#define METRICS_LIBRARY_CONTEXT_CREATE_1_0 "ContextCreate_1_0"
#define METRICS_LIBRARY_CONTEXT_DELETE_1_0 "ContextDelete_1_0"

//////////////////////////////////////////////////////////////////////////
/// @brief Metrics Library current version.
//////////////////////////////////////////////////////////////////////////
#define METRICS_LIBRARY_MAJOR_NUMBER 1
#define METRICS_LIBRARY_MINOR_NUMBER 0
#define METRICS_LIBRARY_BUILD_NUMBER 178

} // namespace MetricsLibraryApi
