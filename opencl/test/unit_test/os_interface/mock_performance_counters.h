/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

//////////////////////////////////////////////////////
// Metrics Library types
//////////////////////////////////////////////////////
using MetricsLibraryApi::ClientApi;
using MetricsLibraryApi::ClientData_1_0;
using MetricsLibraryApi::ClientGen;
using MetricsLibraryApi::ClientType_1_0;
using MetricsLibraryApi::CommandBufferData_1_0;
using MetricsLibraryApi::CommandBufferSize_1_0;
using MetricsLibraryApi::ConfigurationHandle_1_0;
using MetricsLibraryApi::ContextCreateData_1_0;
using MetricsLibraryApi::ContextHandle_1_0;
using MetricsLibraryApi::GpuMemory_1_0;
using MetricsLibraryApi::QueryHandle_1_0;

//////////////////////////////////////////////////////
// MI_REPORT_PERF_COUNT definition for all GENs
//////////////////////////////////////////////////////
struct MI_REPORT_PERF_COUNT {
    uint32_t DwordLength : BITFIELD_RANGE(0, 5);
    uint32_t Reserved_6 : BITFIELD_RANGE(6, 22);
    uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
    uint32_t CommandType : BITFIELD_RANGE(29, 31);
    uint64_t UseGlobalGtt : BITFIELD_RANGE(0, 0);
    uint64_t Reserved_33 : BITFIELD_RANGE(1, 3);
    uint64_t CoreModeEnable : BITFIELD_RANGE(4, 4);
    uint64_t Reserved_37 : BITFIELD_RANGE(5, 5);
    uint64_t MemoryAddress : BITFIELD_RANGE(6, 63);
    uint32_t ReportId;

    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;

    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT = 0x28,
    } MI_COMMAND_OPCODE;

    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;

    inline void init(void) {
        memset(this, 0, sizeof(MI_REPORT_PERF_COUNT));
        DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        MiCommandOpcode = MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT;
        CommandType = COMMAND_TYPE_MI_COMMAND;
    }
};

// clang-format off
//////////////////////////////////////////////////////
// MockMetricsLibrary
//////////////////////////////////////////////////////
class MockMetricsLibrary : public MetricsLibrary {
  public:
    uint32_t openCount    = 0;
    uint32_t contextCount = 0;
    uint32_t queryCount   = 0;
    bool     validOpen    = true;
    bool     validGetData = true;

    // Library open / close functions.
    bool open() override;

    // Context create / destroy functions.
    bool contextCreate              (const ClientType_1_0 &client, ClientOptionsSubDeviceData_1_0 &subDevice, ClientOptionsSubDeviceIndexData_1_0 &subDeviceIndex, ClientOptionsSubDeviceCountData_1_0 &subDeviceCount, ClientData_1_0& clientData, ContextCreateData_1_0 &createData, ContextHandle_1_0 &handle) override;
    bool contextDelete              (const ContextHandle_1_0 &handle) override;

    // HwCounters functions.
    bool hwCountersCreate           (const ContextHandle_1_0 &context, const uint32_t slots, const ConfigurationHandle_1_0 mmio, QueryHandle_1_0 &handle) override;
    bool hwCountersDelete           (const QueryHandle_1_0 &handle) override;
    bool hwCountersGetReport        (const QueryHandle_1_0 &handle, const uint32_t slot, const uint32_t slotsCount, const uint32_t dataSize, void *data) override;
    uint32_t hwCountersGetApiReportSize() override;
    uint32_t hwCountersGetGpuReportSize() override;

    // Command buffer functions.
    bool commandBufferGet           (CommandBufferData_1_0 &data) override;
    bool commandBufferGetSize       (const CommandBufferData_1_0 &commandBufferData, CommandBufferSize_1_0 &commandBufferSize) override;

    // Oa configuration functions.
    bool oaConfigurationCreate      (const ContextHandle_1_0 &context, ConfigurationHandle_1_0 &handle) override { return true; }
    bool oaConfigurationDelete      (const ConfigurationHandle_1_0 &handle) override { return true; }
    bool oaConfigurationActivate    (const ConfigurationHandle_1_0 &handle) override { return true; }
    bool oaConfigurationDeactivate  (const ConfigurationHandle_1_0 &handle) override { return true; }

    // User mmio configuration functions.
    bool userConfigurationCreate    (const ContextHandle_1_0 &context, ConfigurationHandle_1_0 &handle) override { return true; }
    bool userConfigurationDelete    (const ConfigurationHandle_1_0 &handle) override { return true; }
};

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface
//////////////////////////////////////////////////////
class MockMetricsLibraryValidInterface: public MetricsLibraryInterface {
  public:
    uint32_t contextCount                 = 0;
    bool     validCreateConfigurationOa   = true;
    bool     validCreateConfigurationUser = true;
    bool     validActivateConfigurationOa = true;
    bool     validGpuReportSize           = true;

    static StatusCode ML_STDCALL ContextCreate             ( ClientType_1_0 clientType, ContextCreateData_1_0* createData, ContextHandle_1_0* handle ); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ContextDelete             (const ContextHandle_1_0 handle); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL GetParameter              (const ParameterType parameter, ValueType *type, TypedValue_1_0 *value); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL CommandBufferGet          (const CommandBufferData_1_0 *data); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL CommandBufferGetSize      (const CommandBufferData_1_0 *data, CommandBufferSize_1_0 *size); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL QueryCreate               (const QueryCreateData_1_0 *createData, QueryHandle_1_0 *handle); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL QueryDelete               (const QueryHandle_1_0 handle); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationCreate       (const ConfigurationCreateData_1_0 *createData, ConfigurationHandle_1_0 *handle); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationActivate     (const ConfigurationHandle_1_0 handle, const ConfigurationActivateData_1_0 *activateData); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationDeactivate   (const ConfigurationHandle_1_0 handle) { return StatusCode::Success; } // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationDelete       (const ConfigurationHandle_1_0 handle); // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL GetData                   (GetReportData_1_0 *data); // NOLINT(readability-identifier-naming)

    MockMetricsLibraryValidInterface()
    {
        contextCreate                     = &ContextCreate;
        contextDelete                     = &ContextDelete;
        functions.GetParameter            = &GetParameter;
        functions.CommandBufferGet        = &CommandBufferGet;
        functions.CommandBufferGetSize    = &CommandBufferGetSize;
        functions.QueryCreate             = &QueryCreate;
        functions.QueryDelete             = &QueryDelete;
        functions.ConfigurationCreate     = &ConfigurationCreate;
        functions.ConfigurationActivate   = &ConfigurationActivate;
        functions.ConfigurationDeactivate = &ConfigurationDeactivate;
        functions.ConfigurationDelete     = &ConfigurationDelete;
        functions.GetData                 = &GetData;
    }
};

//////////////////////////////////////////////////////
// MockMetricsLibraryInvalidInterface
//////////////////////////////////////////////////////
class MockMetricsLibraryInvalidInterface: public MetricsLibraryInterface {
  public:
    static StatusCode ML_STDCALL ContextCreate             ( ClientType_1_0 clientType, ContextCreateData_1_0* createData, ContextHandle_1_0* handle ){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ContextDelete             (const ContextHandle_1_0 handle){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL GetParameter              (const ParameterType parameter, ValueType *type, TypedValue_1_0 *value){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL CommandBufferGet          (const CommandBufferData_1_0 *data){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL CommandBufferGetSize      (const CommandBufferData_1_0 *data, CommandBufferSize_1_0 *size){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL QueryCreate               (const QueryCreateData_1_0 *createData, QueryHandle_1_0 *handle){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL QueryDelete               (const QueryHandle_1_0 handle){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationCreate       (const ConfigurationCreateData_1_0 *createData, ConfigurationHandle_1_0 *handle){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationActivate     (const ConfigurationHandle_1_0 handle, const ConfigurationActivateData_1_0 *activateData){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationDeactivate   (const ConfigurationHandle_1_0 handle){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL ConfigurationDelete       (const ConfigurationHandle_1_0 handle){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)
    static StatusCode ML_STDCALL GetData                   (GetReportData_1_0 *data){ return StatusCode::Failed;} // NOLINT(readability-identifier-naming)

    MockMetricsLibraryInvalidInterface()
    {
        contextCreate                     = &ContextCreate;
        contextDelete                     = &ContextDelete;
        functions.GetParameter            = &GetParameter;
        functions.CommandBufferGet        = &CommandBufferGet;
        functions.CommandBufferGetSize    = &CommandBufferGetSize;
        functions.QueryCreate             = &QueryCreate;
        functions.QueryDelete             = &QueryDelete;
        functions.ConfigurationCreate     = &ConfigurationCreate;
        functions.ConfigurationActivate   = &ConfigurationActivate;
        functions.ConfigurationDeactivate = &ConfigurationDeactivate;
        functions.ConfigurationDelete     = &ConfigurationDelete;
        functions.GetData                 = &GetData;
    }
};
// clang-format on

//////////////////////////////////////////////////////
// MockMetricsLibraryDll
//////////////////////////////////////////////////////
class MockMetricsLibraryDll : public OsLibrary {
  public:
    bool validContextCreate = true;
    bool validContextDelete = true;
    bool validIsLoaded = true;

    void *getProcAddress(const std::string &procName) override;
    bool isLoaded() override;
};

//////////////////////////////////////////////////////
// MockPerformanceCounters
//////////////////////////////////////////////////////
class MockPerformanceCounters {
  public:
    static std::unique_ptr<PerformanceCounters> create();
};

//////////////////////////////////////////////////////
// PerformanceCountersDeviceFixture
//////////////////////////////////////////////////////
struct PerformanceCountersDeviceFixture {
    virtual void SetUp();    // NOLINT(readability-identifier-naming)
    virtual void TearDown(); // NOLINT(readability-identifier-naming)
    decltype(&PerformanceCounters::create) createFunc;
};

struct MockExecutionEnvironment;
struct RootDeviceEnvironment;
/////////////////////////////////////////////////////
// PerformanceCountersFixture
//////////////////////////////////////////////////////
struct PerformanceCountersFixture {
    PerformanceCountersFixture();
    ~PerformanceCountersFixture();
    virtual void SetUp();    // NOLINT(readability-identifier-naming)
    virtual void TearDown(); // NOLINT(readability-identifier-naming)
    cl_queue_properties queueProperties = {};
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockCommandQueue> queue;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
    std::unique_ptr<OSInterface> osInterface;
};

//////////////////////////////////////////////////////
// PerformanceCountersMetricsLibraryFixture
//////////////////////////////////////////////////////
struct PerformanceCountersMetricsLibraryFixture : PerformanceCountersFixture {

    void SetUp() override;
    void TearDown() override;

    PerformanceCounters *initDeviceWithPerformanceCounters(const bool validMetricsLibraryApi, const bool mockMatricsLibrary);
};

} // namespace NEO
