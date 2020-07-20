/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric.h"

using namespace MetricsLibraryApi;

namespace L0 {
namespace ult {

void MetricDeviceFixture::SetUp() {

    // Call base class.
    DeviceFixture::SetUp();

    // Initialize metric api.
    ze_result_t result = ZE_RESULT_SUCCESS;
    MetricContext::enableMetricApi(result);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto &metricContext = device->getMetricContext();
    metricContext.setInitializationState(ZE_RESULT_SUCCESS);

    // Mock metrics library.
    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricContext));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);

    //  Mock metric enumeration.
    mockMetricEnumeration = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricContext));
    mockMetricEnumeration->setMockedApi(&mockMetricsDiscoveryApi);

    // Metrics Discovery device common settings.
    metricsDeviceParams.Version.MajorNumber = MetricEnumeration::requiredMetricsDiscoveryMajorVersion;
    metricsDeviceParams.Version.MinorNumber = MetricEnumeration::requiredMetricsDiscoveryMinorVersion;
}

void MetricDeviceFixture::TearDown() {

    // Restore original metrics library
    mockMetricsLibrary->setMockedApi(nullptr);
    mockMetricsLibrary.reset();

    // Restore original metric enumeration.
    mockMetricEnumeration->setMockedApi(nullptr);
    mockMetricEnumeration.reset();

    // Call base class.
    DeviceFixture::TearDown();
}

Mock<MetricsLibrary>::Mock(::L0::MetricContext &metricContext) : MetricsLibrary(metricContext) {
}

Mock<MetricsLibrary>::~Mock() {
}

MockMetricsLibraryApi *Mock<MetricsLibrary>::g_mockApi = nullptr;

void Mock<MetricsLibrary>::setMockedApi(MockMetricsLibraryApi *mockedApi) {

    if (mockedApi) {

        //  Mock class used to communicate with metrics library.
        metricsLibrary = &metricContext.getMetricsLibrary();
        metricContext.setMetricsLibrary(*this);

        // Mock metrics library api functions.
        contextCreateFunction = mockedApi->ContextCreate;
        contextDeleteFunction = mockedApi->ContextDelete;

        api.GetParameter = mockedApi->GetParameter;

        api.CommandBufferGet = mockedApi->CommandBufferGet;
        api.CommandBufferGetSize = mockedApi->CommandBufferGetSize;

        api.QueryCreate = mockedApi->QueryCreate;
        api.QueryDelete = mockedApi->QueryDelete;

        api.OverrideCreate = mockedApi->OverrideCreate;
        api.OverrideDelete = mockedApi->OverrideDelete;

        api.MarkerCreate = mockedApi->MarkerCreate;
        api.MarkerDelete = mockedApi->MarkerDelete;

        api.ConfigurationCreate = mockedApi->ConfigurationCreate;
        api.ConfigurationActivate = mockedApi->ConfigurationActivate;
        api.ConfigurationDeactivate = mockedApi->ConfigurationDeactivate;
        api.ConfigurationDelete = mockedApi->ConfigurationDelete;

        api.GetData = mockedApi->GetData;

        // Mock metrics library api.
        Mock<MetricsLibrary>::g_mockApi = mockedApi;

    } else {

        // Restore an original class used to communicate with metrics library.
        metricContext.setMetricsLibrary(*metricsLibrary);
    }
}

StatusCode MockMetricsLibraryApi::ContextCreate(ClientType_1_0 clientType, ContextCreateData_1_0 *createData, ContextHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockContextCreate(clientType, createData, handle);
}

StatusCode MockMetricsLibraryApi::ContextDelete(const ContextHandle_1_0 handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockContextDelete(handle);
}

StatusCode MockMetricsLibraryApi::GetParameter(const ParameterType parameter, ValueType *type, TypedValue_1_0 *value) {
    return Mock<MetricsLibrary>::g_mockApi->MockGetParameter(parameter, type, value);
}

StatusCode MockMetricsLibraryApi::CommandBufferGet(const CommandBufferData_1_0 *data) {
    return Mock<MetricsLibrary>::g_mockApi->MockCommandBufferGet(data);
}

StatusCode MockMetricsLibraryApi::CommandBufferGetSize(const CommandBufferData_1_0 *data, CommandBufferSize_1_0 *size) {
    return Mock<MetricsLibrary>::g_mockApi->MockCommandBufferGetSize(data, size);
}

StatusCode MockMetricsLibraryApi::QueryCreate(const QueryCreateData_1_0 *createData, QueryHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockQueryCreate(createData, handle);
}

StatusCode MockMetricsLibraryApi::QueryDelete(const QueryHandle_1_0 handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockQueryDelete(handle);
}

StatusCode MockMetricsLibraryApi::OverrideCreate(const OverrideCreateData_1_0 *createData, OverrideHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockOverrideCreate(createData, handle);
}

StatusCode MockMetricsLibraryApi::OverrideDelete(const OverrideHandle_1_0 handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockOverrideDelete(handle);
}

StatusCode MockMetricsLibraryApi::MarkerCreate(const MarkerCreateData_1_0 *createData, MarkerHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockMarkerCreate(createData, handle);
}

StatusCode MockMetricsLibraryApi::MarkerDelete(const MarkerHandle_1_0 handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockMarkerDelete(handle);
}

StatusCode MockMetricsLibraryApi::ConfigurationCreate(const ConfigurationCreateData_1_0 *createData, ConfigurationHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockConfigurationCreate(createData, handle);
}

StatusCode MockMetricsLibraryApi::ConfigurationActivate(const ConfigurationHandle_1_0 handle, const ConfigurationActivateData_1_0 *activateData) {
    return Mock<MetricsLibrary>::g_mockApi->MockConfigurationActivate(handle, activateData);
}

StatusCode MockMetricsLibraryApi::ConfigurationDeactivate(const ConfigurationHandle_1_0 handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockConfigurationDeactivate(handle);
}

StatusCode MockMetricsLibraryApi::ConfigurationDelete(const ConfigurationHandle_1_0 handle) {
    return Mock<MetricsLibrary>::g_mockApi->MockConfigurationDeactivate(handle);
}

StatusCode MockMetricsLibraryApi::GetData(GetReportData_1_0 *data) {
    return Mock<MetricsLibrary>::g_mockApi->MockGetData(data);
}

Mock<MetricEnumeration>::Mock(::L0::MetricContext &metricContext) : MetricEnumeration(metricContext) {
}

Mock<MetricEnumeration>::~Mock() {
}

MockMetricsDiscoveryApi *Mock<MetricEnumeration>::g_mockApi = nullptr;

TCompletionCode MockMetricsDiscoveryApi::OpenMetricsDevice(IMetricsDevice_1_5 **device) {
    return Mock<MetricEnumeration>::g_mockApi->MockOpenMetricsDevice(device);
}

TCompletionCode MockMetricsDiscoveryApi::OpenMetricsDeviceFromFile(const char *fileName, void *openParams, IMetricsDevice_1_5 **device) {
    return Mock<MetricEnumeration>::g_mockApi->MockOpenMetricsDeviceFromFile(fileName, openParams, device);
}

TCompletionCode MockMetricsDiscoveryApi::CloseMetricsDevice(IMetricsDevice_1_5 *device) {
    return Mock<MetricEnumeration>::g_mockApi->MockCloseMetricsDevice(device);
}

TCompletionCode MockMetricsDiscoveryApi::SaveMetricsDeviceToFile(const char *fileName, void *saveParams, IMetricsDevice_1_5 *device) {
    return Mock<MetricEnumeration>::g_mockApi->MockSaveMetricsDeviceToFile(fileName, saveParams, device);
}

void Mock<MetricEnumeration>::setMockedApi(MockMetricsDiscoveryApi *mockedApi) {

    if (mockedApi) {

        //  Mock class used to communicate with metrics library.
        metricEnumeration = &metricContext.getMetricEnumeration();
        metricContext.setMetricEnumeration(*this);

        // Mock metrics library api functions.
        openMetricsDevice = mockedApi->OpenMetricsDevice;
        closeMetricsDevice = mockedApi->CloseMetricsDevice;
        openMetricsDeviceFromFile = mockedApi->OpenMetricsDeviceFromFile;

        // Mock metrics library api.
        Mock<MetricEnumeration>::g_mockApi = mockedApi;

    } else {

        // Restore an original class used to communicate with metrics library.
        metricContext.setMetricEnumeration(*metricEnumeration);
    }
}

Mock<MetricQuery>::Mock() {}

Mock<MetricQuery>::~Mock() {}

Mock<MetricQueryPool>::Mock() {}

Mock<MetricQueryPool>::~Mock() {}

} // namespace ult
} // namespace L0

namespace MetricsDiscovery {

IMetricsDevice_1_0::~IMetricsDevice_1_0() {}

TMetricsDeviceParams_1_0 *IMetricsDevice_1_0::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IConcurrentGroup_1_0 *IMetricsDevice_1_0::GetConcurrentGroup(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TGlobalSymbol_1_0 *IMetricsDevice_1_0::GetGlobalSymbol(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TTypedValue_1_0 *IMetricsDevice_1_0::GetGlobalSymbolValueByName(const char *name) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricsDevice_1_0::GetLastError(void) {
    UNRECOVERABLE_IF(true);
    return TCompletionCode::CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricsDevice_1_0::GetGpuCpuTimestamps(uint64_t *, uint64_t *,
                                                        uint32_t *) {
    UNRECOVERABLE_IF(true);
    return TCompletionCode::CC_ERROR_NOT_SUPPORTED;
}

IConcurrentGroup_1_1 *IMetricsDevice_1_1::GetConcurrentGroup(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TMetricsDeviceParams_1_2 *MetricsDiscovery::IMetricsDevice_1_2::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IOverride_1_2 *IMetricsDevice_1_2::GetOverride(unsigned int) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IOverride_1_2 *IMetricsDevice_1_2::GetOverrideByName(char const *) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IConcurrentGroup_1_5 *IMetricsDevice_1_5::GetConcurrentGroup(uint32_t) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IConcurrentGroup_1_0::~IConcurrentGroup_1_0() {}

TConcurrentGroupParams_1_0 *IConcurrentGroup_1_0::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_0 *IConcurrentGroup_1_0::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IConcurrentGroup_1_0::OpenIoStream(IMetricSet_1_0 *metricSet, uint32_t processId, uint32_t *nsTimerPeriod, uint32_t *oaBufferSize) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IConcurrentGroup_1_0::ReadIoStream(uint32_t *reportsCount, char *reportData, uint32_t readFlags) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IConcurrentGroup_1_0::CloseIoStream(void) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IConcurrentGroup_1_0::WaitForReports(uint32_t milliseconds) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IInformation_1_0 *IConcurrentGroup_1_0::GetIoMeasurementInformation(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IInformation_1_0 *IConcurrentGroup_1_0::GetIoGpuContextInformation(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_1 *IConcurrentGroup_1_1::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IConcurrentGroup_1_3::SetIoStreamSamplingType(TSamplingType type) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetricSet_1_5 *IConcurrentGroup_1_5::GetMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_0::~IMetricSet_1_0() {}

TMetricSetParams_1_0 *IMetricSet_1_0::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetric_1_0 *IMetricSet_1_0::GetMetric(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IInformation_1_0 *IMetricSet_1_0::GetInformation(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_0 *IMetricSet_1_0::GetComplementaryMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricSet_1_0::Activate(void) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricSet_1_0::Deactivate(void) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetric_1_0 *IMetricSet_1_0::AddCustomMetric(
    const char *symbolName, const char *shortName, const char *groupName, const char *longName, const char *dxToOglAlias,
    uint32_t usageFlagsMask, uint32_t apiMask, TMetricResultType resultType, const char *resultUnits, TMetricType metricType,
    int64_t loWatermark, int64_t hiWatermark, THwUnitType hwType, const char *ioReadEquation, const char *deltaFunction,
    const char *queryReadEquation, const char *normalizationEquation, const char *maxValueEquation, const char *signalName) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_1 ::~IMetricSet_1_1() {}

TCompletionCode IMetricSet_1_1::SetApiFiltering(uint32_t apiMask) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricSet_1_1::CalculateMetrics(const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out,
                                                 uint32_t outSize, uint32_t *outReportCount, bool enableContextFiltering) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

TCompletionCode IMetricSet_1_1::CalculateIoMeasurementInformation(TTypedValue_1_0 *out, uint32_t outSize) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetricSet_1_4 ::~IMetricSet_1_4() {}

TMetricSetParams_1_4 *IMetricSet_1_4::GetParams(void) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

IMetricSet_1_5 *IMetricSet_1_5::GetComplementaryMetricSet(uint32_t index) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

TCompletionCode IMetricSet_1_5::CalculateMetrics(const unsigned char *rawData, uint32_t rawDataSize, TTypedValue_1_0 *out,
                                                 uint32_t outSize, uint32_t *outReportCount, TTypedValue_1_0 *outMaxValues, uint32_t outMaxValuesSize) {
    UNRECOVERABLE_IF(true);
    return CC_ERROR_NOT_SUPPORTED;
}

IMetric_1_0 ::~IMetric_1_0() {}

TMetricParams_1_0 *IMetric_1_0::GetParams() {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

} // namespace MetricsDiscovery
