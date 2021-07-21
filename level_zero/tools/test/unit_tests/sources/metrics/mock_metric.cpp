/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric.h"

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/mocks/mock_os_library.h"

using namespace MetricsLibraryApi;

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

void MetricContextFixture::SetUp() {

    // Call base class.
    ContextFixture::SetUp();

    // Initialize metric api.
    auto &metricContext = device->getMetricContext();
    metricContext.setInitializationState(ZE_RESULT_SUCCESS);

    // Mock metrics library.
    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricContext));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);
    mockMetricsLibrary->handle = new MockOsLibrary();

    //  Mock metric enumeration.
    mockMetricEnumeration = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricContext));
    mockMetricEnumeration->setMockedApi(&mockMetricsDiscoveryApi);
    mockMetricEnumeration->hMetricsDiscovery = std::make_unique<MockOsLibrary>();

    // Metrics Discovery device common settings.
    metricsDeviceParams.Version.MajorNumber = MetricEnumeration::requiredMetricsDiscoveryMajorVersion;
    metricsDeviceParams.Version.MinorNumber = MetricEnumeration::requiredMetricsDiscoveryMinorVersion;
}

void MetricContextFixture::TearDown() {

    // Restore original metrics library
    delete mockMetricsLibrary->handle;
    mockMetricsLibrary->setMockedApi(nullptr);
    mockMetricsLibrary.reset();

    // Restore original metric enumeration.
    mockMetricEnumeration->setMockedApi(nullptr);
    mockMetricEnumeration.reset();

    // Call base class.
    ContextFixture::TearDown();
}

void MetricContextFixture::openMetricsAdapter() {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce(Return(&adapter));
}

void MetricContextFixture::openMetricsAdapterGroup() {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
}

void MetricMultiDeviceFixture::SetUp() {

    NEO::ImplicitScaling::apiSupport = true;

    MultiDeviceFixture::SetUp();

    devices.resize(driverHandle->devices.size());

    for (uint32_t i = 0; i < driverHandle->devices.size(); i++) {
        devices[i] = driverHandle->devices[i];
    }

    auto &metricContext = devices[0]->getMetricContext();
    metricContext.setInitializationState(ZE_RESULT_SUCCESS);

    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricContext));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);
    mockMetricsLibrary->handle = new MockOsLibrary();

    mockMetricEnumeration = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricContext));
    mockMetricEnumeration->setMockedApi(&mockMetricsDiscoveryApi);
    mockMetricEnumeration->hMetricsDiscovery = std::make_unique<MockOsLibrary>();

    // Metrics Discovery device common settings.
    metricsDeviceParams.Version.MajorNumber = MetricEnumeration::requiredMetricsDiscoveryMajorVersion;
    metricsDeviceParams.Version.MinorNumber = MetricEnumeration::requiredMetricsDiscoveryMinorVersion;
}

void MetricMultiDeviceFixture::TearDown() {

    // Restore original metrics library
    delete mockMetricsLibrary->handle;
    mockMetricsLibrary->setMockedApi(nullptr);
    mockMetricsLibrary.reset();

    // Restore original metric enumeration.
    mockMetricEnumeration->setMockedApi(nullptr);
    mockMetricEnumeration.reset();

    MultiDeviceFixture::TearDown();

    NEO::ImplicitScaling::apiSupport = false;
}

void MetricMultiDeviceFixture::openMetricsAdapter() {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce(Return(&adapter));
}

void MetricMultiDeviceFixture::openMetricsAdapterGroup() {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
}

void MetricMultiDeviceContextFixture::SetUp() {

    NEO::ImplicitScaling::apiSupport = true;

    MultiDeviceFixture::SetUp();

    devices.resize(driverHandle->devices.size());

    for (uint32_t i = 0; i < driverHandle->devices.size(); i++) {
        devices[i] = driverHandle->devices[i];
    }

    // Initialize metric api.
    auto &metricContext = devices[0]->getMetricContext();

    // Mock metrics library.
    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricContext));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);
    mockMetricsLibrary->handle = new MockOsLibrary();

    //  Mock metric enumeration.
    mockMetricEnumeration = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricContext));
    mockMetricEnumeration->setMockedApi(&mockMetricsDiscoveryApi);
    mockMetricEnumeration->hMetricsDiscovery = std::make_unique<MockOsLibrary>();

    // Metrics Discovery device common settings.
    metricsDeviceParams.Version.MajorNumber = MetricEnumeration::requiredMetricsDiscoveryMajorVersion;
    metricsDeviceParams.Version.MinorNumber = MetricEnumeration::requiredMetricsDiscoveryMinorVersion;
}

void MetricMultiDeviceContextFixture::TearDown() {

    // Restore original metrics library
    delete mockMetricsLibrary->handle;
    mockMetricsLibrary->setMockedApi(nullptr);
    mockMetricsLibrary.reset();

    // Restore original metric enumeration.
    mockMetricEnumeration->setMockedApi(nullptr);
    mockMetricEnumeration.reset();

    MultiDeviceFixture::TearDown();

    NEO::ImplicitScaling::apiSupport = false;
}

void MetricMultiDeviceContextFixture::openMetricsAdapter() {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsSubDevice(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .Times(2)
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce(Return(&adapter));
}

void MetricMultiDeviceContextFixture::openMetricsAdapterGroup() {

    EXPECT_CALL(*mockMetricEnumeration, loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
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

Mock<MetricQuery>::Mock() {}

Mock<MetricQuery>::~Mock() {}

Mock<MetricQueryPool>::Mock() {}

Mock<MetricQueryPool>::~Mock() {}

} // namespace ult
} // namespace L0
