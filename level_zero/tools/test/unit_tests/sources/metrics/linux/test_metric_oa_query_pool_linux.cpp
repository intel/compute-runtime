/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace NEO {
namespace SysCalls {
extern int fstatFuncRetVal;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

class MetricQueryPoolLinuxTest : public MetricContextFixture,
                                 public ::testing::Test {
  public:
    void SetUp() override {
        MetricContextFixture::setUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = *device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<DrmMock>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    }

    void TearDown() override {
        MetricContextFixture::tearDown();
    }
};

TEST_F(MetricQueryPoolLinuxTest, givenCorrectArgumentsWhenGetContextDataIsCalledThenReturnsSuccess) {

    ClientData_1_0 clientData = {};
    ContextCreateData_1_0 contextData = {};
    ClientDataLinuxAdapter_1_0 adapter = {};

    adapter.Type = LinuxAdapterType::Last;
    adapter.DrmFileDescriptor = -1;

    clientData.Linux.Adapter = &adapter;
    contextData.ClientData = &clientData;
    contextData.ClientData->Linux.Adapter = &adapter;

    EXPECT_EQ(mockMetricsLibrary->metricsLibraryGetContextData(*device, contextData), true);

    auto &osInterface = *device->getOsInterface();

    EXPECT_EQ(contextData.ClientData->Linux.Adapter->DrmFileDescriptor, osInterface.getDriverModel()->as<Drm>()->getFileDescriptor());
    EXPECT_EQ(contextData.ClientData->Linux.Adapter->Type, LinuxAdapterType::DrmFileDescriptor);
}

TEST_F(MetricQueryPoolLinuxTest, givenValidDeviceHandleWhenFlushCommandBufferCallbackIsCalledThenReturnsSuccess) {

    ClientData_1_0 clientData = {};
    ContextCreateData_1_0 contextData = {};
    ClientDataLinuxAdapter_1_0 adapter = {};

    adapter.Type = LinuxAdapterType::Last;
    adapter.DrmFileDescriptor = -1;

    clientData.Linux.Adapter = &adapter;
    contextData.ClientData = &clientData;
    contextData.ClientData->Linux.Adapter = &adapter;

    EXPECT_EQ(mockMetricsLibrary->metricsLibraryGetContextData(*device, contextData), true);

    auto &callback = mockMetricsLibrary->metricsLibraryGetCallbacks().CommandBufferFlush;

    EXPECT_NE(callback, nullptr);
    EXPECT_EQ(callback(clientData.Handle), StatusCode::Success);
}

TEST_F(MetricQueryPoolLinuxTest, givenInvalidDeviceHandleWhenFlushCommandBufferCallbackIsCalledThenReturnsFailed) {

    ClientData_1_0 clientData = {};
    ContextCreateData_1_0 contextData = {};
    ClientDataLinuxAdapter_1_0 adapter = {};

    adapter.Type = LinuxAdapterType::Last;
    adapter.DrmFileDescriptor = -1;

    clientData.Linux.Adapter = &adapter;
    contextData.ClientData = &clientData;
    contextData.ClientData->Linux.Adapter = &adapter;

    EXPECT_EQ(mockMetricsLibrary->metricsLibraryGetContextData(*device, contextData), true);

    auto &callback = mockMetricsLibrary->metricsLibraryGetCallbacks().CommandBufferFlush;
    clientData.Handle.data = nullptr;

    EXPECT_NE(callback, nullptr);
    EXPECT_EQ(callback(clientData.Handle), StatusCode::Failed);
}

TEST_F(MetricQueryPoolLinuxTest, givenCorrectArgumentsWhenActivateConfigurationIsCalledThenReturnsSuccess) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = &dummyConfigurationHandle;
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    mockMetricsLibrary->g_mockApi->configurationActivationCounter = 1;
    EXPECT_TRUE(mockMetricsLibrary->activateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenCorrectArgumentsWhenActivateConfigurationIsCalledAndMetricLibraryActivateFailsThenReturnsFail) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = &dummyConfigurationHandle;
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    mockMetricsLibrary->g_mockApi->configurationActivationCounter = 1;
    mockMetricsLibrary->g_mockApi->configurationActivateResult = StatusCode::Failed;
    EXPECT_FALSE(mockMetricsLibrary->activateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenInCorrectConfigurationWhenActivateConfigurationIsCalledThenReturnsFail) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = nullptr;
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    EXPECT_FALSE(mockMetricsLibrary->activateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenMetricLibraryIsInIncorrectInitializedStateWhenActivateConfigurationIsCalledThenReturnsFail) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = &dummyConfigurationHandle;
    mockMetricsLibrary->initializationState = ZE_RESULT_ERROR_UNKNOWN;

    mockMetricsLibrary->g_mockApi->configurationActivationCounter = 1;
    EXPECT_FALSE(mockMetricsLibrary->activateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenCorrectArgumentsWhenDeActivateConfigurationIsCalledThenReturnsSuccess) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = &dummyConfigurationHandle;
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    mockMetricsLibrary->g_mockApi->configurationDeactivationCounter = 1;
    EXPECT_TRUE(mockMetricsLibrary->deactivateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenCorrectArgumentsWhenDeActivateConfigurationIsCalledAndMetricLibraryDeActivateFailsThenReturnsFail) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = &dummyConfigurationHandle;
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;

    mockMetricsLibrary->g_mockApi->configurationDeactivationCounter = 1;
    mockMetricsLibrary->g_mockApi->configurationDeactivateResult = StatusCode::Failed;
    EXPECT_FALSE(mockMetricsLibrary->deactivateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenInCorrectConfigurationWhenDeActivateConfigurationIsCalledThenReturnsFail) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = nullptr;
    mockMetricsLibrary->initializationState = ZE_RESULT_SUCCESS;
    mockMetricsLibrary->g_mockApi->configurationDeactivationCounter = 1;
    EXPECT_FALSE(mockMetricsLibrary->deactivateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenMetricLibraryIsInIncorrectInitializedStateWhenDeActivateConfigurationIsCalledThenReturnsFail) {

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = &dummyConfigurationHandle;
    mockMetricsLibrary->initializationState = ZE_RESULT_ERROR_UNKNOWN;

    EXPECT_FALSE(mockMetricsLibrary->deactivateConfiguration(dummyConfigurationHandle));
}

TEST_F(MetricQueryPoolLinuxTest, givenCorrectArgumentsWhenCacheConfigurationIsCalledThenCacheingIsSuccessfull) {

    metricsDeviceParams.ConcurrentGroupsCount = 1;
    Mock<IConcurrentGroup_1_13> metricsConcurrentGroup;
    TConcurrentGroupParams_1_13 metricsConcurrentGroupParams = {};
    metricsConcurrentGroupParams.SymbolName = "OA";
    metricsConcurrentGroupParams.MetricSetsCount = 1;
    metricsConcurrentGroupParams.IoMeasurementInformationCount = 1;

    Mock<MetricsDiscovery::IEquation_1_0> ioReadEquation;
    MetricsDiscovery::TEquationElement_1_0 ioEquationElement = {};
    ioEquationElement.Type = MetricsDiscovery::EQUATION_ELEM_IMM_UINT64;
    ioEquationElement.ImmediateUInt64 = 0;

    ioReadEquation.getEquationElement.push_back(&ioEquationElement);

    Mock<MetricsDiscovery::IInformation_1_0> ioMeasurement;
    MetricsDiscovery::TInformationParams_1_0 oaInformation = {};
    oaInformation.SymbolName = "BufferOverflow";
    oaInformation.IoReadEquation = &ioReadEquation;
    Mock<MetricsDiscovery::IMetricSet_1_13> metricsSet;
    MetricsDiscovery::TMetricSetParams_1_11 metricsSetParams = {};
    metricsSetParams.ApiMask = MetricsDiscovery::API_TYPE_OCL;
    openMetricsAdapter();

    setupDefaultMocksForMetricDevice(metricsDevice);

    metricsDevice.getConcurrentGroupResults.push_back(&metricsConcurrentGroup);

    metricsConcurrentGroup.GetParamsResult = &metricsConcurrentGroupParams;
    metricsConcurrentGroup.getMetricSetResult = &metricsSet;
    metricsConcurrentGroup.GetIoMeasurementInformationResult = &ioMeasurement;
    ioMeasurement.GetParamsResult = &oaInformation;

    metricsSet.GetParamsResult = &metricsSetParams;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    std::vector<zet_metric_group_handle_t> metricGroups;
    metricGroups.resize(metricGroupCount);
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);

    ConfigurationHandle_1_0 dummyConfigurationHandle;
    dummyConfigurationHandle.data = &dummyConfigurationHandle;

    mockMetricsLibrary->deleteAllConfigurations();
    mockMetricsLibrary->cacheConfiguration(metricGroups[0], dummyConfigurationHandle);
    EXPECT_EQ(mockMetricsLibrary->getConfiguration(metricGroups[0]).data, dummyConfigurationHandle.data);
}

TEST_F(MetricQueryPoolLinuxTest, WhenMetricLibraryGetFileNameIsCalledThenCorrectFilenameIsReturned) {
    EXPECT_STREQ(MetricsLibrary::getFilename(), "libigdml.so.1");
}

class MetricEnumerationTestLinux : public MetricContextFixture,
                                   public ::testing::Test {
  public:
    void SetUp() override {
        MetricContextFixture::setUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = *device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<DrmMock>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    }

    void TearDown() override {
        MetricContextFixture::tearDown();
    }
};

TEST_F(MetricEnumerationTestLinux, givenCorrectLinuxDrmAdapterWhenGetMetricsAdapterThenReturnSuccess) {

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_MAJOR_MINOR;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 0;

    openMetricsAdapterGroup();

    setupDefaultMocksForMetricDevice(metricsDevice);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;

    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = adapterParams.SystemId.MajorMinor.Major;
    mockMetricEnumeration->getAdapterIdOutMinor = adapterParams.SystemId.MajorMinor.Minor;
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenCorrectLinuxMinorPrimaryNodeDrmAdapterWhenGetMetricsAdapterThenReturnSuccess) {

    const int32_t drmNodePrimary = 0; // From xf86drm.h
    const int32_t drmMaxDevices = 64; // From drm_drv.c#110

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_MAJOR_MINOR;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 1000 - (drmNodePrimary * drmMaxDevices);

    uint32_t drmMajor = 0;
    uint32_t drmMinor = 1000;

    openMetricsAdapterGroup();

    setupDefaultMocksForMetricDevice(metricsDevice);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;

    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = drmMajor;
    mockMetricEnumeration->getAdapterIdOutMinor = drmMinor;
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenCorrectLinuxMinorRenderNodeDrmAdapterWhenGetMetricsAdapterThenReturnSuccess) {

    const int32_t drmNodeRender = 2;  // From xf86drm.h
    const int32_t drmMaxDevices = 64; // From drm_drv.c#110

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_MAJOR_MINOR;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 1000 - (drmNodeRender * drmMaxDevices);

    uint32_t drmMajor = 0;
    uint32_t drmMinor = 1000;

    openMetricsAdapterGroup();

    setupDefaultMocksForMetricDevice(metricsDevice);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;

    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = drmMajor;
    mockMetricEnumeration->getAdapterIdOutMinor = drmMinor;
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenIcorrectMetricDiscoveryAdapterTypeWhenGetMetricsAdapterThenReturnFail) {

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_LUID;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 0;

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;

    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = adapterParams.SystemId.MajorMinor.Major;
    mockMetricEnumeration->getAdapterIdOutMinor = adapterParams.SystemId.MajorMinor.Minor;
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    setupDefaultMocksForMetricDevice(metricsDevice);

    EXPECT_NE(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenIcorrectMetricDiscoveryAdapterMajorWhenGetMetricsAdapterThenReturnFail) {

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_MAJOR_MINOR;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 0;
    uint32_t incorrectMajor = 1;

    setupDefaultMocksForMetricDevice(metricsDevice);

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;

    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = incorrectMajor;
    mockMetricEnumeration->getAdapterIdOutMinor = adapterParams.SystemId.MajorMinor.Minor;
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    EXPECT_NE(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenIcorrectMetricDiscoveryAdapterMinorWhenGetMetricsAdapterThenReturnFail) {

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_MAJOR_MINOR;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 0;
    uint32_t incorrectMinor = 1;

    setupDefaultMocksForMetricDevice(metricsDevice);

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;

    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = adapterParams.SystemId.MajorMinor.Major;
    mockMetricEnumeration->getAdapterIdOutMinor = incorrectMinor;
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    EXPECT_NE(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenIncorrectOpenMetricDeviceOnAdapterWhenGetMetricsAdapterThenReturnFail) {

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_MAJOR_MINOR;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 0;

    setupDefaultMocksForMetricDevice(metricsDevice);

    mockMetricEnumeration->globalMockApi->adapterGroup = reinterpret_cast<IAdapterGroupLatest *>(&adapterGroup);

    adapterGroup.GetParamsResult = &adapterGroupParams;
    adapterGroup.GetAdapterResult = &adapter;

    adapter.GetParamsResult = &adapterParams;

    mockMetricEnumeration->getAdapterIdOutMajor = adapterParams.SystemId.MajorMinor.Major;
    mockMetricEnumeration->getAdapterIdOutMinor = adapterParams.SystemId.MajorMinor.Minor;
    mockMetricEnumeration->getMetricsAdapterResult = &adapter;

    adapter.openMetricsDeviceResult = TCompletionCode::CC_ERROR_GENERAL;

    EXPECT_NE(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenCorrectDrmFileForFstatWhenGetMetricsAdapterThenReturnSuccess) {

    uint32_t drmMajor = 0;
    uint32_t drmMinor = 0;

    VariableBackup<int> fstatBackup(&NEO::SysCalls::fstatFuncRetVal);
    NEO::SysCalls::fstatFuncRetVal = 0;

    mockMetricEnumeration->getAdapterIdCallBase = true;
    EXPECT_EQ(mockMetricEnumeration->getAdapterId(drmMajor, drmMinor), true);
}

TEST_F(MetricEnumerationTestLinux, givenIncorrectDrmFileForFstatWhenGetMetricsAdapterThenReturnFail) {

    uint32_t drmMajor = 0;
    uint32_t drmMinor = 0;

    VariableBackup<int> fstatBackup(&NEO::SysCalls::fstatFuncRetVal);
    NEO::SysCalls::fstatFuncRetVal = -1;

    mockMetricEnumeration->getAdapterIdCallBase = true;
    EXPECT_EQ(mockMetricEnumeration->getAdapterId(drmMajor, drmMinor), false);
}

} // namespace ult
} // namespace L0
