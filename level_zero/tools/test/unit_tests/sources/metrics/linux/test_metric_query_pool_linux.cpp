/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

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
        MetricContextFixture::SetUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<DrmMock>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    }

    void TearDown() override {
        MetricContextFixture::TearDown();
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

    auto &osInterface = device->getOsInterface();

    EXPECT_EQ(contextData.ClientData->Linux.Adapter->DrmFileDescriptor, osInterface.getDriverModel()->as<Drm>()->getFileDescriptor());
    EXPECT_EQ(contextData.ClientData->Linux.Adapter->Type, LinuxAdapterType::DrmFileDescriptor);
}

class MetricEnumerationTestLinux : public MetricContextFixture,
                                   public ::testing::Test {
  public:
    void SetUp() override {
        MetricContextFixture::SetUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<DrmMock>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    }

    void TearDown() override {
        MetricContextFixture::TearDown();
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

    EXPECT_CALL(adapterGroup, GetParams())
        .Times(1)
        .WillOnce(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(adapterParams.SystemId.MajorMinor.Major), ::testing::SetArgReferee<1>(adapterParams.SystemId.MajorMinor.Minor), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

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

    EXPECT_CALL(adapterGroup, GetParams())
        .WillRepeatedly(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(drmMajor), ::testing::SetArgReferee<1>(drmMinor), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

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

    EXPECT_CALL(adapterGroup, GetParams())
        .WillRepeatedly(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(drmMajor), ::testing::SetArgReferee<1>(drmMinor), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

    EXPECT_EQ(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenIcorrectMetricDiscoveryAdapterTypeWhenGetMetricsAdapterThenReturnFail) {

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_LUID;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 0;

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapterGroup, GetParams())
        .Times(1)
        .WillOnce(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(adapterParams.SystemId.MajorMinor.Major), ::testing::SetArgReferee<1>(adapterParams.SystemId.MajorMinor.Minor), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

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

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapterGroup, GetParams())
        .Times(1)
        .WillOnce(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(incorrectMajor), ::testing::SetArgReferee<1>(adapterParams.SystemId.MajorMinor.Minor), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

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

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapterGroup, GetParams())
        .Times(1)
        .WillOnce(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(adapterParams.SystemId.MajorMinor.Major), ::testing::SetArgReferee<1>(incorrectMinor), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

    EXPECT_NE(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenIcorrectOpenMetricDeviceOnAdapterWhenGetMetricsAdapterThenReturnFail) {

    auto adapterGroupParams = TAdapterGroupParams_1_6{};
    auto adapterParams = TAdapterParams_1_9{};

    adapterGroupParams.AdapterCount = 1;
    adapterParams.SystemId.Type = MetricsDiscovery::TAdapterIdType::ADAPTER_ID_TYPE_MAJOR_MINOR;
    adapterParams.SystemId.MajorMinor.Major = 0;
    adapterParams.SystemId.MajorMinor.Minor = 0;

    EXPECT_CALL(*mockMetricEnumeration->g_mockApi, MockOpenAdapterGroup(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(&adapterGroup), Return(TCompletionCode::CC_OK)));

    EXPECT_CALL(adapterGroup, GetParams())
        .Times(1)
        .WillOnce(Return(&adapterGroupParams));

    EXPECT_CALL(adapterGroup, GetAdapter(_))
        .WillRepeatedly(Return(&adapter));

    EXPECT_CALL(adapter, GetParams())
        .WillRepeatedly(Return(&adapterParams));

    EXPECT_CALL(*mockMetricEnumeration, getAdapterId(_, _))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgReferee<0>(adapterParams.SystemId.MajorMinor.Major), ::testing::SetArgReferee<1>(adapterParams.SystemId.MajorMinor.Minor), Return(true)));

    EXPECT_CALL(*mockMetricEnumeration, getMetricsAdapter())
        .Times(1)
        .WillOnce([&]() { return mockMetricEnumeration->baseGetMetricsAdapter(); });

    EXPECT_CALL(adapter, OpenMetricsDevice(_))
        .Times(1)
        .WillOnce(DoAll(::testing::SetArgPointee<0>(nullptr), Return(TCompletionCode::CC_ERROR_GENERAL)));

    EXPECT_NE(mockMetricEnumeration->openMetricsDiscovery(), ZE_RESULT_SUCCESS);
}

TEST_F(MetricEnumerationTestLinux, givenCorrectDrmFileForFstatWhenGetMetricsAdapterThenReturnSuccess) {

    uint32_t drmMajor = 0;
    uint32_t drmMinor = 0;

    VariableBackup<int> fstatBackup(&NEO::SysCalls::fstatFuncRetVal);
    NEO::SysCalls::fstatFuncRetVal = 0;

    EXPECT_EQ(mockMetricEnumeration->baseGetAdapterId(drmMajor, drmMinor), true);
}

TEST_F(MetricEnumerationTestLinux, givenIncorrectDrmFileForFstatWhenGetMetricsAdapterThenReturnFail) {

    uint32_t drmMajor = 0;
    uint32_t drmMinor = 0;

    VariableBackup<int> fstatBackup(&NEO::SysCalls::fstatFuncRetVal);
    NEO::SysCalls::fstatFuncRetVal = -1;

    EXPECT_EQ(mockMetricEnumeration->baseGetAdapterId(drmMajor, drmMinor), false);
}

} // namespace ult
} // namespace L0
