/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_performance_counters.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
using namespace MetricsLibraryApi;

namespace NEO {
//////////////////////////////////////////////////////
// MockMetricsLibrary::open
//////////////////////////////////////////////////////
bool MockMetricsLibrary::open() {
    if (validOpen) {
        ++openCount;
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::contextCreate
//////////////////////////////////////////////////////
bool MockMetricsLibrary::contextCreate(const ClientType_1_0 &client, ClientData_1_0 &clientData, ContextCreateData_1_0 &createData, ContextHandle_1_0 &handle) {
    if (client.Api != MetricsLibraryApi::ClientApi::OpenCL) {
        return false;
    }

    handle.data = reinterpret_cast<void *>(this);
    ++contextCount;
    return true;
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::contextDelete
//////////////////////////////////////////////////////
bool MockMetricsLibrary::contextDelete(const ContextHandle_1_0 &handle) {
    if (!handle.IsValid()) {
        return false;
    }

    --contextCount;
    return true;
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::hwCountersCreate
//////////////////////////////////////////////////////
bool MockMetricsLibrary::hwCountersCreate(const ContextHandle_1_0 &context, const uint32_t slots, const ConfigurationHandle_1_0 mmio, QueryHandle_1_0 &handle) {
    ++queryCount;
    return true;
};

//////////////////////////////////////////////////////
// MockMetricsLibrary::hwCountersDelete
//////////////////////////////////////////////////////
bool MockMetricsLibrary::hwCountersDelete(const QueryHandle_1_0 &handle) {
    --queryCount;
    return true;
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::hwCountersGetReport
//////////////////////////////////////////////////////
bool MockMetricsLibrary::hwCountersGetReport(const QueryHandle_1_0 &handle, const uint32_t slot, const uint32_t slotsCount, const uint32_t dataSize, void *data) {
    return validGetData;
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::hwCountersGetApiReportSize
//////////////////////////////////////////////////////
uint32_t MockMetricsLibrary::hwCountersGetApiReportSize() {
    return 1;
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::hwCountersGetGpuReportSize
//////////////////////////////////////////////////////
uint32_t MockMetricsLibrary::hwCountersGetGpuReportSize() {
    return sizeof(HwPerfCounter);
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::commandBufferGet
//////////////////////////////////////////////////////
bool MockMetricsLibrary::commandBufferGet(CommandBufferData_1_0 &data) {
    MI_REPORT_PERF_COUNT mirpc = {};
    mirpc.init();
    DEBUG_BREAK_IF(data.Data == nullptr);
    memcpy(data.Data, &mirpc, sizeof(mirpc));
    return true;
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::commandBufferGetSize
//////////////////////////////////////////////////////
bool MockMetricsLibrary::commandBufferGetSize(const CommandBufferData_1_0 &commandBufferData, CommandBufferSize_1_0 &commandBufferSize) {
    commandBufferSize.GpuMemorySize = sizeof(MI_REPORT_PERF_COUNT);
    return true;
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::getProcAddress
//////////////////////////////////////////////////////
void *MockMetricsLibraryDll::getProcAddress(const std::string &procName) {
    if (procName == METRICS_LIBRARY_CONTEXT_CREATE_1_0) {
        return validContextCreate
                   ? reinterpret_cast<void *>(&MockMetricsLibraryValidInterface::ContextCreate)
                   : nullptr;
    } else if (procName == METRICS_LIBRARY_CONTEXT_DELETE_1_0) {
        return validContextDelete
                   ? reinterpret_cast<void *>(&MockMetricsLibraryValidInterface::ContextDelete)
                   : nullptr;
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////
// MockMetricsLibrary::isLoaded
//////////////////////////////////////////////////////
bool MockMetricsLibraryDll::isLoaded() {
    return validIsLoaded;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::ContextCreate
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::ContextCreate(ClientType_1_0 clientType, ContextCreateData_1_0 *createData, ContextHandle_1_0 *handle) {

    // Validate input.
    EXPECT_EQ(clientType.Api, ClientApi::OpenCL);

    // Library handle.
    auto library = new MockMetricsLibraryValidInterface();
    handle->data = library;
    EXPECT_TRUE(handle->IsValid());

    // Context count.
    library->contextCount++;
    EXPECT_EQ(library->contextCount, 1u);

    return handle->IsValid()
               ? StatusCode::Success
               : StatusCode::Failed;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::ContextDelete
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::ContextDelete(const ContextHandle_1_0 handle) {

    auto validHandle = handle.IsValid();
    auto library = static_cast<MockMetricsLibraryValidInterface *>(handle.data);

    // Validate input.
    EXPECT_TRUE(validHandle);
    EXPECT_TRUE(validHandle);
    EXPECT_EQ(--library->contextCount, 0u);

    // Delete handle.
    delete library;

    return validHandle
               ? StatusCode::Success
               : StatusCode::IncorrectObject;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryInterface::QueryCreate
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::QueryCreate(const QueryCreateData_1_0 *createData, QueryHandle_1_0 *handle) {

    EXPECT_NE(handle, nullptr);
    EXPECT_NE(createData, nullptr);
    EXPECT_GE(createData->Slots, 1u);
    EXPECT_TRUE(createData->HandleContext.IsValid());
    EXPECT_EQ(createData->Type, ObjectType::QueryHwCounters);

    handle->data = new uint32_t(0);
    return StatusCode::Success;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::QueryDelete
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::QueryDelete(const QueryHandle_1_0 handle) {
    EXPECT_TRUE(handle.IsValid());
    delete (uint32_t *)handle.data;
    return StatusCode::Success;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::CommandBufferGetSize
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::CommandBufferGetSize(const CommandBufferData_1_0 *data, CommandBufferSize_1_0 *size) {
    auto library = static_cast<MockMetricsLibraryValidInterface *>(data->HandleContext.data);
    EXPECT_NE(data, nullptr);
    EXPECT_TRUE(data->HandleContext.IsValid());
    EXPECT_TRUE(data->QueryHwCounters.Handle.IsValid());
    EXPECT_EQ(data->Type, GpuCommandBufferType::Render);
    EXPECT_EQ(data->CommandsType, ObjectType::QueryHwCounters);
    EXPECT_NE(size, nullptr);

    size->GpuMemorySize = library->validGpuReportSize
                              ? 123
                              : 0;
    return library->validGpuReportSize
               ? StatusCode::Success
               : StatusCode::Failed;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::CommandBufferGet
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::CommandBufferGet(const CommandBufferData_1_0 *data) {
    EXPECT_NE(data, nullptr);
    EXPECT_TRUE(data->HandleContext.IsValid());
    EXPECT_TRUE(data->QueryHwCounters.Handle.IsValid());
    EXPECT_EQ(data->Type, GpuCommandBufferType::Render);
    EXPECT_EQ(data->CommandsType, ObjectType::QueryHwCounters);
    EXPECT_NE(data->Data, nullptr);
    EXPECT_GT(data->Size, 0u);
    return StatusCode::Success;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::CommandBufferGet
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::GetParameter(const ParameterType parameter, ValueType *type, TypedValue_1_0 *value) {
    EXPECT_NE(type, nullptr);
    EXPECT_NE(value, nullptr);
    switch (parameter) {
    case ParameterType::QueryHwCountersReportApiSize:
        *type = ValueType::Uint32;
        value->ValueUInt32 = 123;
        break;
    case ParameterType::QueryHwCountersReportGpuSize:
        *type = ValueType::Uint32;
        value->ValueUInt32 = 123;
        break;
    default:
        EXPECT_TRUE(false);
        break;
    }
    return StatusCode::Success;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::ConfigurationCreate
//////////////////////////////////////////////////////
StatusCode ML_STDCALL MockMetricsLibraryValidInterface::ConfigurationCreate(const ConfigurationCreateData_1_0 *createData, ConfigurationHandle_1_0 *handle) {
    EXPECT_NE(createData, nullptr);
    EXPECT_NE(handle, nullptr);
    EXPECT_TRUE(createData->HandleContext.IsValid());

    const bool validType = (createData->Type == ObjectType::ConfigurationHwCountersOa) ||
                           (createData->Type == ObjectType::ConfigurationHwCountersUser);

    // Mock overrides
    auto api = static_cast<MockMetricsLibraryValidInterface *>(createData->HandleContext.data);
    if (!api->validCreateConfigurationOa && (createData->Type == ObjectType::ConfigurationHwCountersOa)) {
        return StatusCode::Failed;
    }

    if (!api->validCreateConfigurationUser && (createData->Type == ObjectType::ConfigurationHwCountersUser)) {
        return StatusCode::Failed;
    }

    EXPECT_TRUE(validType);

    handle->data = api;
    return StatusCode::Success;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::ConfigurationActivate
//////////////////////////////////////////////////////
StatusCode ML_STDCALL MockMetricsLibraryValidInterface::ConfigurationActivate(const ConfigurationHandle_1_0 handle, const ConfigurationActivateData_1_0 *activateData) {
    auto api = static_cast<MockMetricsLibraryValidInterface *>(handle.data);
    return api->validActivateConfigurationOa
               ? StatusCode::Success
               : StatusCode::Failed;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::ConfigurationDelete
//////////////////////////////////////////////////////
StatusCode ML_STDCALL MockMetricsLibraryValidInterface::ConfigurationDelete(const ConfigurationHandle_1_0 handle) {
    EXPECT_TRUE(handle.IsValid());

    return StatusCode::Success;
}

//////////////////////////////////////////////////////
// MockMetricsLibraryValidInterface::GetData
//////////////////////////////////////////////////////
StatusCode MockMetricsLibraryValidInterface::GetData(GetReportData_1_0 *data) {
    EXPECT_NE(data, nullptr);
    EXPECT_EQ(data->Type, ObjectType::QueryHwCounters);
    EXPECT_TRUE(data->Query.Handle.IsValid());
    EXPECT_GE(data->Query.Slot, 0u);
    EXPECT_GT(data->Query.SlotsCount, 0u);
    EXPECT_NE(data->Query.Data, nullptr);
    EXPECT_GT(data->Query.DataSize, 0u);
    return StatusCode::Success;
}

//////////////////////////////////////////////////////
// PerformanceCountersDeviceFixture::SetUp
//////////////////////////////////////////////////////
void PerformanceCountersDeviceFixture::SetUp() {
    createFunc = Device::createPerformanceCountersFunc;
    Device::createPerformanceCountersFunc = MockPerformanceCounters::create;
}

//////////////////////////////////////////////////////
// PerformanceCountersDeviceFixture::TearDown
//////////////////////////////////////////////////////
void PerformanceCountersDeviceFixture::TearDown() {
    Device::createPerformanceCountersFunc = createFunc;
}

//////////////////////////////////////////////////////
// PerformanceCountersMetricsLibraryFixture::SetUp
//////////////////////////////////////////////////////
void PerformanceCountersMetricsLibraryFixture::SetUp() {
    PerformanceCountersFixture::SetUp();
}

//////////////////////////////////////////////////////
// PerformanceCountersMetricsLibraryFixture::TearDown
//////////////////////////////////////////////////////
void PerformanceCountersMetricsLibraryFixture::TearDown() {
    device->setPerfCounters(nullptr);
    PerformanceCountersFixture::TearDown();
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::PerformanceCountersFixture
//////////////////////////////////////////////////////
PerformanceCountersFixture::PerformanceCountersFixture() {
    executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    rootDeviceEnvironment = std::make_unique<RootDeviceEnvironment>(*executionEnvironment);
}

//////////////////////////////////////////////////////
// PerformanceCountersFixture::~PerformanceCountersFixture
//////////////////////////////////////////////////////
PerformanceCountersFixture::~PerformanceCountersFixture() {
}

//////////////////////////////////////////////////////
// PerformanceCountersMetricsLibraryFixture::createPerformanceCounters
//////////////////////////////////////////////////////
void PerformanceCountersMetricsLibraryFixture::createPerformanceCounters(const bool validMetricsLibraryApi, const bool mockMetricsLibrary) {
    performanceCountersBase = MockPerformanceCounters::create(&device->getDevice());
    auto metricsLibraryInterface = performanceCountersBase->getMetricsLibraryInterface();
    auto metricsLibraryDll = std::make_unique<MockMetricsLibraryDll>();
    EXPECT_NE(performanceCountersBase, nullptr);
    EXPECT_NE(metricsLibraryInterface, nullptr);

    device->setPerfCounters(performanceCountersBase.get());

    // Attached mock version of metrics library interface.
    if (mockMetricsLibrary) {
        performanceCountersBase->setMetricsLibraryInterface(std::make_unique<MockMetricsLibrary>());
        metricsLibraryInterface = performanceCountersBase->getMetricsLibraryInterface();
    } else {
        performanceCountersBase->setMetricsLibraryInterface(std::make_unique<MetricsLibrary>());
        metricsLibraryInterface = performanceCountersBase->getMetricsLibraryInterface();
    }

    if (validMetricsLibraryApi) {
        metricsLibraryInterface->api = std::make_unique<MockMetricsLibraryValidInterface>();
        metricsLibraryInterface->osLibrary = std::move(metricsLibraryDll);
    } else {
        metricsLibraryDll->validContextCreate = false;
        metricsLibraryDll->validContextDelete = false;
        metricsLibraryDll->validIsLoaded = false;
        metricsLibraryInterface->api = std::make_unique<MockMetricsLibraryInvalidInterface>();
        metricsLibraryInterface->osLibrary = std::move(metricsLibraryDll);
    }

    EXPECT_NE(metricsLibraryInterface->api, nullptr);
}
} // namespace NEO
