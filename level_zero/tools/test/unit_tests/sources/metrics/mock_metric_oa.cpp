/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/source/metrics/metric_oa_streamer_imp.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

using namespace MetricsLibraryApi;

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

class MockIpSamplingOsInterface : public MetricIpSamplingOsInterface {

  public:
    ~MockIpSamplingOsInterface() override = default;
    ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) override {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t stopMeasurement() override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) override { return ZE_RESULT_ERROR_UNKNOWN; }
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) override { return 0; }
    uint32_t getUnitReportSize() override { return 0; }
    bool isNReportsAvailable() override { return false; }
    bool isDependencyAvailable() override { return false; }
};

void MetricContextFixture::SetUp() {

    // Call base class.
    ContextFixture::SetUp();

    // Initialize metric api.
    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricSource.setInitializationState(ZE_RESULT_SUCCESS);

    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface =
        std::unique_ptr<MetricIpSamplingOsInterface>(new MockIpSamplingOsInterface());
    auto &ipMetricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    ipMetricSource.setMetricOsInterface(metricIpSamplingOsInterface);

    // Mock metrics library.
    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricSource));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);
    mockMetricsLibrary->handle = new MockOsLibrary();

    //  Mock metric enumeration.
    mockMetricEnumeration = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricSource));
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

    EXPECT_CALL(adapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
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

    EXPECT_CALL(adapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
}

void MetricMultiDeviceFixture::SetUp() {
    DebugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixture::SetUp();

    devices.resize(driverHandle->devices.size());

    for (uint32_t i = 0; i < driverHandle->devices.size(); i++) {
        devices[i] = driverHandle->devices[i];
    }

    // Initialize metric api.
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricSource.setInitializationState(ZE_RESULT_SUCCESS);

    // Mock metrics library.
    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricSource));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);
    mockMetricsLibrary->handle = new MockOsLibrary();

    //  Mock metric enumeration.
    mockMetricEnumeration = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricSource));
    mockMetricEnumeration->setMockedApi(&mockMetricsDiscoveryApi);
    mockMetricEnumeration->hMetricsDiscovery = std::make_unique<MockOsLibrary>();

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());
    mockMetricEnumerationSubDevices.resize(subDeviceCount);
    mockMetricsLibrarySubDevices.resize(subDeviceCount);

    for (uint32_t i = 0; i < subDeviceCount; i++) {
        auto &metricsSubDeviceContext = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        mockMetricEnumerationSubDevices[i] = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricsSubDeviceContext));
        mockMetricEnumerationSubDevices[i]->setMockedApi(&mockMetricsDiscoveryApi);
        mockMetricEnumerationSubDevices[i]->hMetricsDiscovery = std::make_unique<MockOsLibrary>();

        mockMetricsLibrarySubDevices[i] = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricsSubDeviceContext));
        mockMetricsLibrarySubDevices[i]->setMockedApi(&mockMetricsLibraryApi);
        mockMetricsLibrarySubDevices[i]->handle = new MockOsLibrary();

        metricsSubDeviceContext.setInitializationState(ZE_RESULT_SUCCESS);
    }
    // Metrics Discovery device common settings.
    metricsDeviceParams.Version.MajorNumber = MetricEnumeration::requiredMetricsDiscoveryMajorVersion;
    metricsDeviceParams.Version.MinorNumber = MetricEnumeration::requiredMetricsDiscoveryMinorVersion;
}

void MetricMultiDeviceFixture::TearDown() {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    for (uint32_t i = 0; i < subDeviceCount; i++) {

        mockMetricEnumerationSubDevices[i]->setMockedApi(nullptr);
        mockMetricEnumerationSubDevices[i].reset();

        delete mockMetricsLibrarySubDevices[i]->handle;
        mockMetricsLibrarySubDevices[i]->setMockedApi(nullptr);
        mockMetricsLibrarySubDevices[i].reset();
    }

    mockMetricEnumerationSubDevices.clear();
    mockMetricsLibrarySubDevices.clear();

    // Restore original metrics library
    delete mockMetricsLibrary->handle;
    mockMetricsLibrary->setMockedApi(nullptr);
    mockMetricsLibrary.reset();

    // Restore original metric enumeration.
    mockMetricEnumeration->setMockedApi(nullptr);
    mockMetricEnumeration.reset();

    MultiDeviceFixture::TearDown();
}

void MetricMultiDeviceFixture::openMetricsAdapter() {

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

    EXPECT_CALL(adapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
}

void MetricMultiDeviceFixture::openMetricsAdapterSubDevice(uint32_t subDeviceIndex) {

    EXPECT_CALL(*mockMetricEnumerationSubDevices[subDeviceIndex], loadMetricsDiscovery())
        .Times(0);

    EXPECT_CALL(*mockMetricEnumerationSubDevices[subDeviceIndex]->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .Times(1)
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .Times(0);

    EXPECT_CALL(*mockMetricEnumerationSubDevices[subDeviceIndex], getMetricsAdapter())
        .Times(1)
        .WillOnce(Return(&adapter));

    EXPECT_CALL(adapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
}

void MetricMultiDeviceFixture::openMetricsAdapterDeviceAndSubDeviceNoCountVerify(uint32_t subDeviceIndex) {

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .WillRepeatedly(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsSubDevice(_, _))
        .WillRepeatedly(DoAll(::testing::SetArgPointee<1>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapterGroup, Close())
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(*mockMetricEnumerationSubDevices[subDeviceIndex]->g_mockApi, MockOpenAdapterGroup(_))
        .WillRepeatedly(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .WillRepeatedly(DoAll(::testing::SetArgPointee<0>(&metricsDevice), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapter, CloseMetricsDevice(_))
        .WillRepeatedly(Return(TCompletionCode::CC_OK));

    EXPECT_CALL(*mockMetricEnumerationSubDevices[subDeviceIndex], getMetricsAdapter())
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapterGroup, Close())
        .WillRepeatedly(Return(TCompletionCode::CC_OK));
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

    EXPECT_CALL(adapterGroup, Close())
        .Times(1)
        .WillOnce(Return(TCompletionCode::CC_OK));
}

void MetricStreamerMultiDeviceFixture::cleanup(zet_device_handle_t &hDevice, zet_metric_streamer_handle_t &hStreamer) {

    OaMetricStreamerImp *pStreamerImp = static_cast<OaMetricStreamerImp *>(MetricStreamer::fromHandle(hStreamer));
    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);

    for (size_t index = 0; index < deviceImp.subDevices.size(); index++) {
        zet_metric_streamer_handle_t metricStreamerSubDeviceHandle = pStreamerImp->getMetricStreamers()[index];
        OaMetricStreamerImp *pStreamerSubDevImp = static_cast<OaMetricStreamerImp *>(MetricStreamer::fromHandle(metricStreamerSubDeviceHandle));
        auto device = deviceImp.subDevices[index];
        auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto &metricsLibrary = metricSource.getMetricsLibrary();
        metricSource.setMetricStreamer(nullptr);
        metricsLibrary.release();
        delete pStreamerSubDevImp;
    }
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricSource.setMetricStreamer(nullptr);
    delete pStreamerImp;
}

Mock<MetricsLibrary>::Mock(::L0::OaMetricSourceImp &metricSource) : MetricsLibrary(metricSource) {
}

Mock<MetricsLibrary>::~Mock() {
}

MockMetricsLibraryApi *Mock<MetricsLibrary>::g_mockApi = nullptr;

void Mock<MetricsLibrary>::setMockedApi(MockMetricsLibraryApi *mockedApi) {

    if (mockedApi) {

        //  Mock class used to communicate with metrics library.
        metricsLibrary = &metricSource.getMetricsLibrary();
        metricSource.setMetricsLibrary(*this);

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
        metricSource.setMetricsLibrary(*metricsLibrary);
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
    return Mock<MetricsLibrary>::g_mockApi->MockConfigurationDelete(handle);
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
