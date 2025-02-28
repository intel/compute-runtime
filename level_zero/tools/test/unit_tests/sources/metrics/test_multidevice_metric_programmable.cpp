/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_multidevice_programmable.h"
#include "level_zero/tools/source/metrics/metric_multidevice_programmable.inl"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa_programmable.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"

namespace L0 {
namespace ult {

class OaMultiDeviceMetricProgrammableFixture : public MultiDeviceFixture,
                                               public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;
    MockIAdapterGroup1x13 mockAdapterGroup{};

    std::vector<Device *> subDevices = {};
    Device *rootDevice = nullptr;
    DebugManagerStateRestore restorer;
    MockIConcurrentGroup1x13 mockConcurrentGroup{};

    void getMetricProgrammable(zet_metric_programmable_exp_handle_t &programmable);
    void getMetricFromProgrammable(zet_metric_programmable_exp_handle_t programmable, zet_metric_handle_t &metricHandle);
    void createMetricGroupFromMetric(zet_metric_handle_t metric, std::vector<zet_metric_group_handle_t> &metricGroupHandles);
};

void OaMultiDeviceMetricProgrammableFixture::TearDown() {
    MultiDeviceFixture::tearDown();

    auto &deviceImp = *static_cast<DeviceImp *>(rootDevice);
    auto &rootDeviceOaSourceImp = deviceImp.getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    auto rootDeviceMetricEnumeration = static_cast<MetricEnumeration *>(&rootDeviceOaSourceImp.getMetricEnumeration());
    rootDeviceMetricEnumeration->cleanupExtendedMetricInformation();

    for (auto &subDevice : subDevices) {
        auto &subDeviceImp = *static_cast<DeviceImp *>(subDevice);
        auto &subDeviceOaSourceImp = subDeviceImp.getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
        auto subDeviceMetricEnumeration = static_cast<MetricEnumeration *>(&subDeviceOaSourceImp.getMetricEnumeration());
        subDeviceMetricEnumeration->cleanupExtendedMetricInformation();
    }
}

void OaMultiDeviceMetricProgrammableFixture::getMetricProgrammable(zet_metric_programmable_exp_handle_t &programmable) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
}

void OaMultiDeviceMetricProgrammableFixture::getMetricFromProgrammable(zet_metric_programmable_exp_handle_t programmable, zet_metric_handle_t &metricHandle) {
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle));
    EXPECT_NE(metricHandle, nullptr);
}

void OaMultiDeviceMetricProgrammableFixture::createMetricGroupFromMetric(zet_metric_handle_t metricHandle, std::vector<zet_metric_group_handle_t> &metricGroupHandles) {

    metricGroupHandles.clear();
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    metricGroupHandles.resize(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));

    for (auto &metricGroup : metricGroupHandles) {
        EXPECT_NE(metricGroup, nullptr);
    }
}

void OaMultiDeviceMetricProgrammableFixture::SetUp() {
    debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixture::numRootDevices = 1;
    MultiDeviceFixture::numSubDevices = 3;
    MultiDeviceFixture::setUp();

    mockAdapterGroup.mockParams.Version.MajorNumber = 1;
    mockAdapterGroup.mockParams.Version.MinorNumber = 13;

    rootDevice = driverHandle->devices[0];
    auto &deviceImp = *static_cast<DeviceImp *>(rootDevice);
    auto &rootDeviceOaSourceImp = deviceImp.getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    rootDeviceOaSourceImp.setInitializationState(ZE_RESULT_SUCCESS);
    auto rootDeviceMetricEnumeration = static_cast<MetricEnumeration *>(&rootDeviceOaSourceImp.getMetricEnumeration());
    rootDeviceMetricEnumeration->setInitializationState(ZE_RESULT_SUCCESS);
    const uint32_t subDeviceCount = static_cast<uint32_t>(deviceImp.subDevices.size());

    for (uint32_t i = 0; i < subDeviceCount; i++) {
        subDevices.push_back(deviceImp.subDevices[i]);
        auto deviceContext = new MetricDeviceContext(*deviceImp.subDevices[i]);
        auto oaMetricSource = static_cast<OaMetricSourceImp *>(&deviceContext->getMetricSource<OaMetricSourceImp>());
        auto metricEnumeration = static_cast<MetricEnumeration *>(&oaMetricSource->getMetricEnumeration());
        metricEnumeration->setAdapterGroup(&mockAdapterGroup);
        oaMetricSource->setInitializationState(ZE_RESULT_SUCCESS);
        metricEnumeration->setInitializationState(ZE_RESULT_SUCCESS);

        MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = static_cast<MetricsDiscovery::IConcurrentGroup_1_13 &>(mockConcurrentGroup);
        EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
        static_cast<DeviceImp *>(deviceImp.subDevices[i])->metricContext.reset(deviceContext);
    }
}

using OaMultiDeviceMetricProgrammableTests = OaMultiDeviceMetricProgrammableFixture;

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricProgrammableIsSupportedThenValidHandlesAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetPropertiesExp(programmable, &properties));
    EXPECT_STREQ(properties.component, "GroupName");
    EXPECT_STREQ(properties.description, "LongName");
    EXPECT_STREQ(properties.name, "SymbolName");
    EXPECT_EQ(properties.domain, 1u);
    EXPECT_EQ(properties.tierNumber, 1u);
    EXPECT_EQ(properties.samplingType, METRICS_SAMPLING_TYPE_TIME_EVENT_BASED);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricProgrammableThenCorrectParamerterInfoIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    ASSERT_NE(programmable, nullptr);
    zet_metric_programmable_param_info_exp_t parameterInfo[4];
    uint32_t parameterCount = 6;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, parameterInfo));
    EXPECT_EQ(parameterCount, 4u);

    EXPECT_EQ(parameterInfo[0].defaultValue.ui64, UINT64_MAX);
    EXPECT_STREQ(parameterInfo[0].name, "mockParam1");
    EXPECT_EQ(parameterInfo[0].type, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_DISAGGREGATION);
    EXPECT_EQ(parameterInfo[0].valueInfoCount, 1u);
    EXPECT_EQ(parameterInfo[0].valueInfoType, ZET_VALUE_INFO_TYPE_EXP_UINT32);

    EXPECT_EQ(parameterInfo[1].defaultValue.ui64, 0u);
    EXPECT_STREQ(parameterInfo[1].name, "mockParam2");
    EXPECT_EQ(parameterInfo[1].type, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_LATENCY);
    EXPECT_EQ(parameterInfo[1].valueInfoCount, 1u);
    EXPECT_EQ(parameterInfo[1].valueInfoType, ZET_VALUE_INFO_TYPE_EXP_UINT64);

    EXPECT_EQ(parameterInfo[2].defaultValue.ui64, 0u);
    EXPECT_STREQ(parameterInfo[2].name, "mockParam3");
    EXPECT_EQ(parameterInfo[2].type, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_NORMALIZATION_UTILIZATION);
    EXPECT_EQ(parameterInfo[2].valueInfoCount, 1u);
    EXPECT_EQ(parameterInfo[2].valueInfoType, ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE);

    EXPECT_EQ(parameterInfo[3].defaultValue.ui64, 0u);
    EXPECT_STREQ(parameterInfo[3].name, "mockParam4");
    EXPECT_EQ(parameterInfo[3].type, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_NORMALIZATION_BYTES);
    EXPECT_EQ(parameterInfo[3].valueInfoCount, 1u);
    EXPECT_EQ(parameterInfo[3].valueInfoType, ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricProgrammableThenCorrectParamerterValueInfoIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    ASSERT_NE(programmable, nullptr);
    std::vector<zet_metric_programmable_param_info_exp_t> parameterInfo(4);
    uint32_t parameterCount = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, parameterInfo.data()));
    uint32_t valueInfoCount = 2;
    zet_metric_programmable_param_value_info_exp_t valueInfo{};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 0, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui32, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 1, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui64, 2u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 2, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Min, 3u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Max, 4u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 3, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Min, 5u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Max, 6u);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricProgrammableWhenCreateMetricIsCalledThenValidMetricHandleIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle));
    EXPECT_NE(metricHandle, nullptr);
    zet_metric_properties_t metricProperties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricHandle, &metricProperties));
    EXPECT_STREQ(metricProperties.name, metricName);
    EXPECT_STREQ(metricProperties.description, metricDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricsWhenMetricGroupIsCreatedFromThemThenValidMetricGroupIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle));
    EXPECT_NE(metricHandle, nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupHandles[0], nullptr);
    zet_metric_group_properties_t metricGroupProperties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupGetProperties(metricGroupHandles[0], &metricGroupProperties));

    for (uint32_t index = 0; index < metricGroupCount; index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[index]));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricsWhenMetricGroupCreationFailsInOneOfTheSubDeviceThenErrorIsReturned) {
    MockIMetricSet1x13::addMetricCallCount = 0;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList.resize(3);
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[1] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[2] = MetricsDiscovery::CC_ERROR_GENERAL;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle));
    EXPECT_NE(metricHandle, nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupHandles[0], nullptr);

    for (uint32_t index = 0; index < metricGroupCount; index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[index]));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));

    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList.resize(1);
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    MockIMetricSet1x13::addMetricCallCount = 0;
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricsWhenMetricGroupCreationReturnsDifferentGroupCountForDifferentSubDevicesThenErrorIsReturned) {

    MockIMetricSet1x13::addMetricCallCount = 0;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList.resize(7);
    // 1st Group creation - OK
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[1] = MetricsDiscovery::CC_OK;

    // 2nd Group creation - OK
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[2] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[3] = MetricsDiscovery::CC_OK;

    // 3rd Group creation
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[4] = MetricsDiscovery::CC_OK;
    // fail during adding 2nd metric
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[5] = MetricsDiscovery::CC_ERROR_GENERAL;
    // New Metric group created here
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[6] = MetricsDiscovery::CC_OK;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[1]));
    EXPECT_NE(metricHandle[0], nullptr);
    EXPECT_NE(metricHandle[1], nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 2, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);

    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 2, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupHandles[0], nullptr);
    EXPECT_EQ(metricGroupCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[1]));

    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList.resize(1);
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    MockIMetricSet1x13::addMetricCallCount = 0;
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupThenMetricGetReturnsSuccess) {

    zet_metric_programmable_exp_handle_t programmable{};
    getMetricProgrammable(programmable);

    zet_metric_handle_t metricHandle{};
    getMetricFromProgrammable(programmable, metricHandle);

    std::vector<zet_metric_group_handle_t> metricGroups{};
    createMetricGroupFromMetric(metricHandle, metricGroups);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGet(metricGroups[0], &count, nullptr));
    // Additional metrics added during close
    EXPECT_EQ(count, 3u);
    zet_metric_handle_t getMetric{};
    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGet(metricGroups[0], &count, &getMetric));
    EXPECT_NE(getMetric, nullptr);
    for (auto &metricGroup : metricGroups) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroup));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenSubdevicesAddDifferentMetricCountsThenErrorIsObserved) {

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
    mockConcurrentGroup.mockMetricSet.mockMetricCountList.resize(4);
    // First Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[0] = 1;
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[1] = 1;
    // Second Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[2] = 3;
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[3] = 3;
    mockConcurrentGroup.mockMetricSet.mockInformationCount = 0;

    zet_metric_programmable_exp_handle_t programmable{};
    getMetricProgrammable(programmable);
    zet_metric_handle_t metricHandle{};
    getMetricFromProgrammable(programmable, metricHandle);
    std::vector<zet_metric_group_handle_t> metricGroupHandles{};

    metricGroupHandles.clear();
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    metricGroupHandles.resize(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenMetricGroupCloseFailsForOneOfTheGroupThenErrorIsObserved) {

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
    mockConcurrentGroup.mockMetricSet.mockMetricCountList.resize(6);
    // Group0: First Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[0] = 1;
    // Group0: Second Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[1] = 1;
    // Group0: Third Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[2] = 1;

    // Group1: First Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[3] = 1;
    // Group1: Second Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[4] = 1;
    // Group1: Third Subdevice metric count
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[5] = 3;
    mockConcurrentGroup.mockMetricSet.mockInformationCount = 0;

    zet_metric_programmable_exp_handle_t programmable{};
    getMetricProgrammable(programmable);
    zet_metric_handle_t metricHandle{};
    getMetricFromProgrammable(programmable, metricHandle);
    std::vector<zet_metric_group_handle_t> metricGroupHandles{};

    metricGroupHandles.clear();
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    metricGroupHandles.resize(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenCloseDoesNotAddNewMetricsThenMetricCountIsCorrect) {

    mockConcurrentGroup.mockMetricSet.mockMetricCountList.resize(1);
    mockConcurrentGroup.mockMetricSet.mockMetricCountList[0] = 1;
    mockConcurrentGroup.mockMetricSet.mockInformationCount = 0;

    zet_metric_programmable_exp_handle_t programmable{};
    getMetricProgrammable(programmable);

    zet_metric_handle_t metricHandle{};
    getMetricFromProgrammable(programmable, metricHandle);

    std::vector<zet_metric_group_handle_t> metricGroups{};
    createMetricGroupFromMetric(metricHandle, metricGroups);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGet(metricGroups[0], &count, nullptr));

    EXPECT_EQ(count, 1u);
    zet_metric_handle_t getMetric{};
    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGet(metricGroups[0], &count, &getMetric));
    EXPECT_NE(getMetric, nullptr);
    for (auto &metricGroup : metricGroups) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroup));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupThenAddAndRemoveMetricReturnsSuccess) {
    zet_metric_programmable_exp_handle_t programmable{};
    getMetricProgrammable(programmable);
    zet_metric_handle_t metricHandle{};
    getMetricFromProgrammable(programmable, metricHandle);
    std::vector<zet_metric_group_handle_t> metricGroups{};
    createMetricGroupFromMetric(metricHandle, metricGroups);

    zet_metric_handle_t metricHandle1{};
    getMetricFromProgrammable(programmable, metricHandle1);

    size_t errorStringSize = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroups[0], metricHandle1, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroups[0], metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroups[0], metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroups[0]));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroups[0], metricHandle1, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroups[0], metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroups[0]));

    for (uint32_t index = 0; index < static_cast<uint32_t>(metricGroups.size()); index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroups[index]));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenAddMetricFailsForOneOfTheSubDevicesThenReturnFailure) {
    zet_metric_programmable_exp_handle_t programmable{};
    getMetricProgrammable(programmable);
    zet_metric_handle_t metricHandle{};
    getMetricFromProgrammable(programmable, metricHandle);
    std::vector<zet_metric_group_handle_t> metricGroups{};
    createMetricGroupFromMetric(metricHandle, metricGroups);

    zet_metric_handle_t metricHandle1{};
    getMetricFromProgrammable(programmable, metricHandle1);

    size_t errorStringSize = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroups[0], metricHandle1, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroups[0], metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroups[0]));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroups[0], metricHandle1, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroups[0], metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroups[0]));

    for (uint32_t index = 0; index < static_cast<uint32_t>(metricGroups.size()); index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroups[index]));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenAddAndRemoveMetricIsCalledWithIncorrectMetricThenReturnFailure) {
    zet_metric_programmable_exp_handle_t programmable{};
    getMetricProgrammable(programmable);
    zet_metric_handle_t metricHandle{};
    getMetricFromProgrammable(programmable, metricHandle);
    std::vector<zet_metric_group_handle_t> metricGroups{};
    createMetricGroupFromMetric(metricHandle, metricGroups);

    MockMetricSource metricSource{};
    MockMetric mockMetric(metricSource);
    mockMetric.setMultiDevice(false);
    mockMetric.setPredefined(false);

    size_t errorStringSize = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroups[0], mockMetric.toHandle(), &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupRemoveMetricExp(metricGroups[0], mockMetric.toHandle()));

    mockMetric.setMultiDevice(true);
    mockMetric.setPredefined(true);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroups[0], mockMetric.toHandle(), &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupRemoveMetricExp(metricGroups[0], mockMetric.toHandle()));

    for (uint32_t index = 0; index < static_cast<uint32_t>(metricGroups.size()); index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroups[index]));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenAddMetricIsCalledAndOneSubDeviceFailsThenReturnFailure) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[1]));
    EXPECT_NE(metricHandle[0], nullptr);
    EXPECT_NE(metricHandle[1], nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_GT(metricGroupCount, 0u);

    // Prepare AddMetric return status for failure
    MockIMetricSet1x13::addMetricCallCount = 0;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList.resize(3);
    // 1st Group
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    // 2nd Group
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[1] = MetricsDiscovery::CC_OK;
    // 3rd Group
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[2] = MetricsDiscovery::CC_ERROR_GENERAL;

    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroupHandles[0], metricHandle[1], &errorStringSize, nullptr));

    for (uint32_t index = 0; index < metricGroupCount; index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[index]));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[1]));

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenRemoveMetricIsCalledAndOneSubDeviceFailsThenReturnFailure) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[1]));
    EXPECT_NE(metricHandle[0], nullptr);
    EXPECT_NE(metricHandle[1], nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 2, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 2, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_GT(metricGroupCount, 0u);

    MockIMetricSet1x13::removeMetricCallCount = 0;
    mockConcurrentGroup.mockMetricSet.mockRemoveMetricReturnCodeList.resize(3);
    // 1st Group
    mockConcurrentGroup.mockMetricSet.mockRemoveMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    // 2nd Group
    mockConcurrentGroup.mockMetricSet.mockRemoveMetricReturnCodeList[1] = MetricsDiscovery::CC_OK;
    // 3rd Group
    mockConcurrentGroup.mockMetricSet.mockRemoveMetricReturnCodeList[2] = MetricsDiscovery::CC_ERROR_GENERAL;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetMetricGroupRemoveMetricExp(metricGroupHandles[0], metricHandle[1]));

    for (uint32_t index = 0; index < metricGroupCount; index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[index]));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[1]));

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
    MockIMetricSet1x13::addMetricCallCount = 0;
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenCloseIsCalledAndOneSubDeviceFailsThenReturnFailure) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[1]));
    EXPECT_NE(metricHandle[0], nullptr);
    EXPECT_NE(metricHandle[1], nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_GT(metricGroupCount, 0u);
    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandles[0], metricHandle[1], &errorStringSize, nullptr));

    MockIMetricSet1x13::closeCallCount = 0;
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList.resize(3);
    // 1st Group
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList[0] = MetricsDiscovery::CC_OK;
    // 2nd Group
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList[1] = MetricsDiscovery::CC_ERROR_GENERAL;
    // 3rd Group
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList[2] = MetricsDiscovery::CC_OK;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetMetricGroupCloseExp(metricGroupHandles[0]));

    for (uint32_t index = 0; index < metricGroupCount; index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[index]));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[1]));

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricGroupWhenCreateIsCalledAndCloseForOneSubDeviceFailsThenReturnFailure) {

    MockIMetricSet1x13::closeCallCount = 0;
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList.resize(3);
    // 1st Group
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList[0] = MetricsDiscovery::CC_OK;
    // 2nd Group
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList[1] = MetricsDiscovery::CC_OK;
    // 3rd Group
    mockConcurrentGroup.mockMetricSet.mockCloseReturnCodeList[2] = MetricsDiscovery::CC_ERROR_GENERAL;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle[1]));
    EXPECT_NE(metricHandle[0], nullptr);
    EXPECT_NE(metricHandle[1], nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle[1]));

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMultiDeviceMetricProgrammableTests, givenMultiDeviceMetricsWhenMetricIsDestroyedBeforeMetricGroupDestroyThenErrorInUseIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetExp(rootDevice->toHandle(), &count, &programmable));
    const char *metricName = "MetricName";
    const char *metricDesc = "MetricDesc";
    uint32_t metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, metricName, metricDesc, &metricHandleCount, &metricHandle));
    EXPECT_NE(metricHandle, nullptr);
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_GT(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceCreateMetricGroupsFromMetricsExp(rootDevice->toHandle(), 1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupHandles[0], nullptr);
    zet_metric_group_properties_t metricGroupProperties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupGetProperties(metricGroupHandles[0], &metricGroupProperties));

    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, zetMetricDestroyExp(metricHandle));

    for (uint32_t index = 0; index < metricGroupCount; index++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[index]));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
}

struct MockMetricProgrammable : public MetricProgrammable {

    MockMetricSource metricSource{};
    std::vector<MockMetric *> allocatedMetricObjects{};

    ~MockMetricProgrammable() override {
        for (auto &metric : allocatedMetricObjects) {
            delete metric;
        }
        allocatedMetricObjects.clear();
    }

    ze_result_t createMetricReturnStatus = ZE_RESULT_SUCCESS;
    uint32_t createMetricReturnHandleCount = 2;

    ze_result_t getProperties(zet_metric_programmable_exp_properties_t *pProperties) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t getParamInfo(uint32_t *pParameterCount, zet_metric_programmable_param_info_exp_t *pParameterInfo) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t getParamValueInfo(uint32_t parameterOrdinal, uint32_t *pValueInfoCount,
                                  zet_metric_programmable_param_value_info_exp_t *pValueInfo) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t createMetric(zet_metric_programmable_param_value_exp_t *pParameterValues,
                             uint32_t parameterCount, const char name[ZET_MAX_METRIC_NAME],
                             const char description[ZET_MAX_METRIC_DESCRIPTION],
                             uint32_t *pMetricHandleCount, zet_metric_handle_t *phMetricHandles) override {

        if (*pMetricHandleCount == 0) {
            *pMetricHandleCount = createMetricReturnHandleCount;
            return createMetricReturnStatus;
        }

        *pMetricHandleCount = std::min(*pMetricHandleCount, createMetricReturnHandleCount);
        prepareMetricObjects(*pMetricHandleCount);

        for (uint32_t index = 0; index < *pMetricHandleCount; index++) {
            *phMetricHandles++ = allocatedMetricObjects[index];
        }

        return createMetricReturnStatus;
    }

    void prepareMetricObjects(uint32_t metricHandleCount) {

        bool additionalAllocationNecessary = metricHandleCount > static_cast<uint32_t>(allocatedMetricObjects.size());
        if (additionalAllocationNecessary) {
            uint32_t additionalAllocations = metricHandleCount - static_cast<uint32_t>(allocatedMetricObjects.size());
            for (uint32_t index = 0; index < additionalAllocations; index++) {
                auto mockMetric = new MockMetric(metricSource);
                mockMetric->destroyReturn = ZE_RESULT_SUCCESS;
                allocatedMetricObjects.push_back(mockMetric);
            }
        }
    }
};

TEST(HomogeneousMultiDeviceMetricProgrammableTest, givenSubDevicesReturnNonHomogenousMetricsWhenCreateMetricsIsCalledThenErrorIsReturned) {

    MockMetricSource mockMetricSource{};
    MockMetricProgrammable mockSubDeviceProgrammables[2];
    std::vector<MetricProgrammable *> subDeviceProgrammables{};
    subDeviceProgrammables.push_back(&mockSubDeviceProgrammables[0]);
    subDeviceProgrammables.push_back(&mockSubDeviceProgrammables[1]);

    MetricProgrammable *multiDeviceProgrammable = HomogeneousMultiDeviceMetricProgrammable::create(mockMetricSource, subDeviceProgrammables);
    const char *name = "name";
    const char *desc = "desc";
    uint32_t metricHandleCount = 0;
    mockSubDeviceProgrammables[0].createMetricReturnHandleCount = 2;
    mockSubDeviceProgrammables[1].createMetricReturnHandleCount = 3;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, multiDeviceProgrammable->createMetric(nullptr, 0, name, desc, &metricHandleCount, nullptr));

    mockSubDeviceProgrammables[0].createMetricReturnHandleCount = 2;
    mockSubDeviceProgrammables[1].createMetricReturnHandleCount = 3;
    metricHandleCount = 4;
    std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, multiDeviceProgrammable->createMetric(nullptr, 0, name, desc, &metricHandleCount, metricHandles.data()));

    mockSubDeviceProgrammables[0].createMetricReturnHandleCount = 3;
    mockSubDeviceProgrammables[0].prepareMetricObjects(3);
    static_cast<MockMetric *>(mockSubDeviceProgrammables[0].allocatedMetricObjects[0])->destroyReturn = ZE_RESULT_SUCCESS;
    mockSubDeviceProgrammables[1].createMetricReturnHandleCount = 3;
    metricHandleCount = 4;
    EXPECT_EQ(ZE_RESULT_SUCCESS, multiDeviceProgrammable->createMetric(nullptr, 0, name, desc, &metricHandleCount, metricHandles.data()));
    EXPECT_EQ(metricHandleCount, 3u);

    for (uint32_t index = 0; index < metricHandleCount; index++) {
        zetMetricDestroyExp(metricHandles[index]);
    }

    delete multiDeviceProgrammable;
}

TEST(HomogeneousMultiDeviceMetricProgrammableTest, givenSubDevicesReturnErrorWhenCreateMetricsIsCalledThenErrorIsReturned) {

    MockMetricSource mockMetricSource{};
    MockMetricProgrammable mockSubDeviceProgrammables[2];
    std::vector<MetricProgrammable *> subDeviceProgrammables{};
    subDeviceProgrammables.push_back(&mockSubDeviceProgrammables[0]);
    subDeviceProgrammables.push_back(&mockSubDeviceProgrammables[1]);

    MetricProgrammable *multiDeviceProgrammable = HomogeneousMultiDeviceMetricProgrammable::create(mockMetricSource, subDeviceProgrammables);
    const char *name = "name";
    const char *desc = "desc";
    uint32_t metricHandleCount = 0;
    mockSubDeviceProgrammables[0].createMetricReturnHandleCount = 2;
    mockSubDeviceProgrammables[1].createMetricReturnHandleCount = 3;
    mockSubDeviceProgrammables[1].createMetricReturnStatus = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, multiDeviceProgrammable->createMetric(nullptr, 0, name, desc, &metricHandleCount, nullptr));

    mockSubDeviceProgrammables[0].createMetricReturnHandleCount = 2;
    mockSubDeviceProgrammables[1].createMetricReturnHandleCount = 3;
    mockSubDeviceProgrammables[1].createMetricReturnStatus = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    metricHandleCount = 4;
    std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, multiDeviceProgrammable->createMetric(nullptr, 0, name, desc, &metricHandleCount, metricHandles.data()));

    delete multiDeviceProgrammable;
}

TEST(HomogeneousMultiDeviceMetricProgrammableTest, givenSubDevicesReturnZeroMetricWhenCreateMetricsIsCalledThenSuccessIsReturned) {

    MockMetricSource mockMetricSource{};
    MockMetricProgrammable mockSubDeviceProgrammables[2];
    std::vector<MetricProgrammable *> subDeviceProgrammables{};
    subDeviceProgrammables.push_back(&mockSubDeviceProgrammables[0]);
    subDeviceProgrammables.push_back(&mockSubDeviceProgrammables[1]);

    MetricProgrammable *multiDeviceProgrammable = HomogeneousMultiDeviceMetricProgrammable::create(mockMetricSource, subDeviceProgrammables);
    const char *name = "name";
    const char *desc = "desc";
    uint32_t metricHandleCount = 0;
    mockSubDeviceProgrammables[0].createMetricReturnHandleCount = 0;
    mockSubDeviceProgrammables[1].createMetricReturnHandleCount = 0;
    mockSubDeviceProgrammables[1].createMetricReturnStatus = ZE_RESULT_SUCCESS;
    EXPECT_EQ(ZE_RESULT_SUCCESS, multiDeviceProgrammable->createMetric(nullptr, 0, name, desc, &metricHandleCount, nullptr));

    mockSubDeviceProgrammables[0].createMetricReturnHandleCount = 0;
    mockSubDeviceProgrammables[1].createMetricReturnHandleCount = 0;
    mockSubDeviceProgrammables[1].createMetricReturnStatus = ZE_RESULT_SUCCESS;
    metricHandleCount = 4;
    std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, multiDeviceProgrammable->createMetric(nullptr, 0, name, desc, &metricHandleCount, metricHandles.data()));

    delete multiDeviceProgrammable;
}

TEST(HomogeneousMultiDeviceMetricTest, givenMultiDeviceMetricWhenDestroyIsCalledThenUnsupportedFeatureIsReturned) {

    MockMetricSource mockSource;
    MockMetric mockMetric(mockSource);
    std::vector<MetricImp *> mockMetrics{&mockMetric};
    MultiDeviceMetricImp *multiDevMetric = static_cast<MultiDeviceMetricImp *>(
        MultiDeviceMetricImp::create(mockSource, mockMetrics));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, multiDevMetric->destroy());
    delete multiDevMetric;
}

struct MultiDeviceCreatedMetricGroupManagerTest : public MultiDeviceFixture,

                                                  public ::testing::Test {
    void SetUp() override {
        MultiDeviceFixture::numRootDevices = 1;
        MultiDeviceFixture::numSubDevices = 2;
        MultiDeviceFixture::setUp();
        rootDevice = driverHandle->devices[0];
        rootDeviceMetricContext = &(static_cast<DeviceImp *>(rootDevice)->getMetricDeviceContext());
    }

    void TearDown() override {
        MultiDeviceFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
    Device *rootDevice = nullptr;
    MetricDeviceContext *rootDeviceMetricContext = nullptr;
};

struct DummyMetricGroup : public MockMetricGroup {
    static DummyMetricGroup *create(MetricSource &metricSource,
                                    std::vector<MetricGroupImp *> &subDeviceMetricGroups,
                                    std::vector<MultiDeviceMetricImp *> &multiDeviceMetrics) {
        return nullptr;
    }
};

TEST_F(MultiDeviceCreatedMetricGroupManagerTest, givenMetricFromSubDeviceIsUsedWhenCreateMultipleFromHomogenousMetricsIsCalledThenErrorIsReturned) {

    MockMetricSource mockMetricSource{};
    MockMetric metric(mockMetricSource);
    metric.setMultiDevice(false);

    uint32_t metricGroupFromMetricsCount = 0;
    auto metricHandle = metric.toHandle();
    std::vector<zet_metric_group_handle_t> metricGroups{};
    std::vector<zet_metric_handle_t> metrics(1);
    metrics[0] = metricHandle;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, MultiDeviceCreatedMetricGroupManager::createMultipleMetricGroupsFromMetrics<DummyMetricGroup>(
                                                    *rootDeviceMetricContext, mockMetricSource,
                                                    metrics, "name", "desc", &metricGroupFromMetricsCount, metricGroups));
    EXPECT_EQ(metricGroupFromMetricsCount, 0u);
}

} // namespace ult
} // namespace L0
