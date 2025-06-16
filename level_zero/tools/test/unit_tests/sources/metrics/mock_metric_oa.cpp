/*
 * Copyright (C) 2020-2025 Intel Corporation
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
#include "level_zero/tools/source/metrics/os_interface_metric.h"

using namespace MetricsLibraryApi;

namespace L0 {
namespace ult {

void MetricContextFixture::setUp() {

    debugManager.flags.DisableProgrammableMetricsSupport.set(1);

    // Call base class.
    DeviceFixture::setUp();

    // Initialize metric api.
    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricSource.setInitializationState(ZE_RESULT_SUCCESS);

    mockIpSamplingOsInterface = new MockIpSamplingOsInterface();
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface =
        std::unique_ptr<MetricIpSamplingOsInterface>(mockIpSamplingOsInterface);
    auto &ipMetricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    ipMetricSource.setMetricOsInterface(metricIpSamplingOsInterface);
    device->getMetricDeviceContext().setMetricsCollectionAllowed(true);

    // Mock metrics library.
    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricSource));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);
    mockMetricsLibrary->handle = std::make_unique<MockOsLibrary>();

    //  Mock metric enumeration.
    mockMetricEnumeration = std::unique_ptr<Mock<MetricEnumeration>>(new (std::nothrow) Mock<MetricEnumeration>(metricSource));
    mockMetricEnumeration->setMockedApi(&mockMetricsDiscoveryApi);
    mockMetricEnumeration->hMetricsDiscovery = std::make_unique<MockOsLibrary>();

    // Metrics Discovery device common settings.
    metricsDeviceParams.Version.MajorNumber = MetricEnumeration::requiredMetricsDiscoveryMajorVersion;
    metricsDeviceParams.Version.MinorNumber = MetricEnumeration::requiredMetricsDiscoveryMinorVersion;
}

void MetricContextFixture::tearDown() {

    // Restore original metrics library
    mockMetricsLibrary->setMockedApi(nullptr);
    mockMetricsLibrary.reset();

    // Restore original metric enumeration.
    mockMetricEnumeration->setMockedApi(nullptr);
    mockMetricEnumeration.reset();

    // Call base class.
    DeviceFixture::tearDown();
}

void MetricContextFixture::openMetricsAdapter() {

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    adapter.openMetricsDeviceOutDevice = &metricsDevice;
}

void MetricContextFixture::openMetricsAdapterGroup() {

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);
    adapter.openMetricsDeviceOutDevice = &metricsDevice;
}

void MetricContextFixture::setupDefaultMocksForMetricDevice(Mock<IMetricsDevice_1_13> &metricDevice) {
    metricDevice.GetParamsResult = &metricsDeviceParams;
}

void MetricMultiDeviceFixture::setUp() {
    debugManager.flags.DisableProgrammableMetricsSupport.set(1);
    debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixture::setUp();

    devices.resize(driverHandle->devices.size());

    for (uint32_t i = 0; i < driverHandle->devices.size(); i++) {
        devices[i] = driverHandle->devices[i];
    }

    // Initialize metric api.
    auto &metricSource = devices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    metricSource.setInitializationState(ZE_RESULT_SUCCESS);
    devices[0]->getMetricDeviceContext().setMetricsCollectionAllowed(true);

    // Mock metrics library.
    mockMetricsLibrary = std::unique_ptr<Mock<MetricsLibrary>>(new (std::nothrow) Mock<MetricsLibrary>(metricSource));
    mockMetricsLibrary->setMockedApi(&mockMetricsLibraryApi);
    mockMetricsLibrary->handle = std::make_unique<MockOsLibrary>();

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
        mockMetricsLibrarySubDevices[i]->handle = std::make_unique<MockOsLibrary>();

        metricsSubDeviceContext.setInitializationState(ZE_RESULT_SUCCESS);
        deviceImp.subDevices[i]->getMetricDeviceContext().setMetricsCollectionAllowed(true);
    }
    // Metrics Discovery device common settings.
    metricsDeviceParams.Version.MajorNumber = MetricEnumeration::requiredMetricsDiscoveryMajorVersion;
    metricsDeviceParams.Version.MinorNumber = MetricEnumeration::requiredMetricsDiscoveryMinorVersion;
}

void MetricMultiDeviceFixture::tearDown() {

    auto &deviceImp = *static_cast<DeviceImp *>(devices[0]);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    for (uint32_t i = 0; i < subDeviceCount; i++) {

        mockMetricEnumerationSubDevices[i]->setMockedApi(nullptr);
        mockMetricEnumerationSubDevices[i].reset();

        mockMetricsLibrarySubDevices[i]->setMockedApi(nullptr);
        mockMetricsLibrarySubDevices[i].reset();
    }

    mockMetricEnumerationSubDevices.clear();
    mockMetricsLibrarySubDevices.clear();

    // Restore original metrics library
    mockMetricsLibrary->setMockedApi(nullptr);
    mockMetricsLibrary.reset();

    // Restore original metric enumeration.
    mockMetricEnumeration->setMockedApi(nullptr);
    mockMetricEnumeration.reset();

    MultiDeviceFixture::tearDown();
}

void MetricMultiDeviceFixture::openMetricsAdapter() {

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    adapter.openMetricsSubDeviceOutDevice = &metricsDevice;
    adapter.openMetricsDeviceOutDevice = &metricsDevice;
}

void MetricMultiDeviceFixture::openMetricsAdapterSubDevice(uint32_t subDeviceIndex) {

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);

    mockMetricEnumerationSubDevices[subDeviceIndex]->getMetricsAdapterResult = &adapter;

    adapter.openMetricsDeviceOutDevice = &metricsDevice;
}

void MetricMultiDeviceFixture::openMetricsAdapterDeviceAndSubDeviceNoCountVerify(uint32_t subDeviceIndex) {

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;
    mockMetricEnumerationSubDevices[subDeviceIndex]->getMetricsAdapterResult = &adapter;

    adapter.openMetricsDeviceOutDevice = &metricsDevice;
    adapter.openMetricsSubDeviceOutDevice = &metricsDevice;
}

void MetricMultiDeviceFixture::openMetricsAdapterGroup() {

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);
    adapter.openMetricsDeviceOutDevice = &metricsDevice;
}

void MetricMultiDeviceFixture::setupDefaultMocksForMetricDevice(Mock<IMetricsDevice_1_13> &metricDevice) {
    metricDevice.GetParamsResult = &metricsDeviceParams;
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
        auto &actualMetricsLibrary = metricSource.getMetricsLibraryObject();
        actualMetricsLibrary.release();
        actualMetricsLibrary.reset(this);

        // Mock metrics library api functions.
        contextCreateFunction = mockedApi->contextCreate;
        contextDeleteFunction = mockedApi->contextDelete;

        api.GetParameter = mockedApi->getParameter;

        api.CommandBufferGet = mockedApi->commandBufferGet;
        api.CommandBufferGetSize = mockedApi->commandBufferGetSize;

        api.QueryCreate = mockedApi->queryCreate;
        api.QueryDelete = mockedApi->queryDelete;

        api.OverrideCreate = mockedApi->overrideCreate;
        api.OverrideDelete = mockedApi->overrideDelete;

        api.MarkerCreate = mockedApi->markerCreate;
        api.MarkerDelete = mockedApi->markerDelete;

        api.ConfigurationCreate = mockedApi->configurationCreate;
        api.ConfigurationActivate = mockedApi->configurationActivate;
        api.ConfigurationDeactivate = mockedApi->configurationDeactivate;
        api.ConfigurationDelete = mockedApi->configurationDelete;

        api.GetData = mockedApi->getData;

        // Mock metrics library api.
        Mock<MetricsLibrary>::g_mockApi = mockedApi;

    } else {

        // Restore an original class used to communicate with metrics library.
        auto &actualMetricsLibrary = metricSource.getMetricsLibraryObject();
        actualMetricsLibrary.release();
        actualMetricsLibrary.reset(metricsLibrary);
    }
}

StatusCode MockMetricsLibraryApi::contextCreate(ClientType_1_0 clientType, ContextCreateData_1_0 *createData, ContextHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->contextCreateImpl(clientType, createData, handle);
}

StatusCode MockMetricsLibraryApi::contextDelete(const ContextHandle_1_0 handle) {
    return StatusCode::Success;
}

StatusCode MockMetricsLibraryApi::getParameter(const ParameterType parameter, ValueType *type, TypedValue_1_0 *value) {
    return Mock<MetricsLibrary>::g_mockApi->getParameterImpl(parameter, type, value);
}

StatusCode MockMetricsLibraryApi::commandBufferGet(const CommandBufferData_1_0 *data) {
    return Mock<MetricsLibrary>::g_mockApi->commandBufferGetImpl(data);
}

StatusCode MockMetricsLibraryApi::commandBufferGetSize(const CommandBufferData_1_0 *data, CommandBufferSize_1_0 *size) {
    return Mock<MetricsLibrary>::g_mockApi->commandBufferGetSizeImpl(data, size);
}

StatusCode MockMetricsLibraryApi::queryCreate(const QueryCreateData_1_0 *createData, QueryHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->queryCreateImpl(createData, handle);
}

StatusCode MockMetricsLibraryApi::queryDelete(const QueryHandle_1_0 handle) {
    return StatusCode::Success;
}

StatusCode MockMetricsLibraryApi::overrideCreate(const OverrideCreateData_1_0 *createData, OverrideHandle_1_0 *handle) {
    return StatusCode::Success;
}

StatusCode MockMetricsLibraryApi::overrideDelete(const OverrideHandle_1_0 handle) {
    return StatusCode::Success;
}

StatusCode MockMetricsLibraryApi::markerCreate(const MarkerCreateData_1_0 *createData, MarkerHandle_1_0 *handle) {
    return StatusCode::Success;
}

StatusCode MockMetricsLibraryApi::markerDelete(const MarkerHandle_1_0 handle) {
    return StatusCode::Success;
}

StatusCode MockMetricsLibraryApi::configurationCreate(const ConfigurationCreateData_1_0 *createData, ConfigurationHandle_1_0 *handle) {
    return Mock<MetricsLibrary>::g_mockApi->configurationCreateImpl(createData, handle);
}

StatusCode MockMetricsLibraryApi::configurationActivate(const ConfigurationHandle_1_0 handle, const ConfigurationActivateData_1_0 *activateData) {
    return Mock<MetricsLibrary>::g_mockApi->configurationActivateImpl(handle, activateData);
}

StatusCode MockMetricsLibraryApi::configurationDeactivate(const ConfigurationHandle_1_0 handle) {
    return Mock<MetricsLibrary>::g_mockApi->configurationDeactivateImpl(handle);
}

StatusCode MockMetricsLibraryApi::configurationDelete(const ConfigurationHandle_1_0 handle) {
    return StatusCode::Success;
}

StatusCode MockMetricsLibraryApi::getData(GetReportData_1_0 *data) {
    return Mock<MetricsLibrary>::g_mockApi->getDataImpl(data);
}

Mock<MetricQuery>::Mock() {}

Mock<MetricQuery>::~Mock() {}

Mock<MetricQueryPool>::Mock() {}

Mock<MetricQueryPool>::~Mock() {}

} // namespace ult
} // namespace L0
