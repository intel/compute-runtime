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
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa_programmable.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"

namespace L0 {
namespace ult {

class OaMetricProgrammableFixture : public DeviceFixture,
                                    public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;
    std::unique_ptr<MetricDeviceContext> deviceContext = nullptr;
    OaMetricSourceImp *oaMetricSource = nullptr;
    MetricEnumeration *metricEnumeration = nullptr;
    MockIAdapterGroup1x13 mockAdapterGroup{};
    void disableProgrammableMetricsSupport();
    DebugManagerStateRestore restorer;
};

void OaMetricProgrammableFixture::TearDown() {
    DeviceFixture::tearDown();
    deviceContext.reset();
}

void OaMetricProgrammableFixture::SetUp() {
    DeviceFixture::setUp();

    mockAdapterGroup.mockParams.Version.MajorNumber = 1;
    mockAdapterGroup.mockParams.Version.MinorNumber = 13;
    deviceContext = std::make_unique<MetricDeviceContext>(*device);
    deviceContext->setMetricsCollectionAllowed(true);
    oaMetricSource = static_cast<OaMetricSourceImp *>(&deviceContext->getMetricSource<OaMetricSourceImp>());
    metricEnumeration = static_cast<MetricEnumeration *>(&oaMetricSource->getMetricEnumeration());
    metricEnumeration->setAdapterGroup(&mockAdapterGroup);

    oaMetricSource->setInitializationState(ZE_RESULT_SUCCESS);
    metricEnumeration->setInitializationState(ZE_RESULT_SUCCESS);
}

void OaMetricProgrammableFixture::disableProgrammableMetricsSupport() {

    deviceContext.reset();
    debugManager.flags.DisableProgrammableMetricsSupport.set(1);
    mockAdapterGroup.mockParams.Version.MajorNumber = 1;
    mockAdapterGroup.mockParams.Version.MinorNumber = 13;
    deviceContext = std::make_unique<MetricDeviceContext>(*device);
    oaMetricSource = static_cast<OaMetricSourceImp *>(&deviceContext->getMetricSource<OaMetricSourceImp>());
    metricEnumeration = static_cast<MetricEnumeration *>(&oaMetricSource->getMetricEnumeration());
    metricEnumeration->setAdapterGroup(&mockAdapterGroup);

    oaMetricSource->setInitializationState(ZE_RESULT_SUCCESS);
    metricEnumeration->setInitializationState(ZE_RESULT_SUCCESS);
}

using OaMetricProgrammableTests = OaMetricProgrammableFixture;

TEST_F(OaMetricProgrammableTests, givenMetricProgrammableIsSupportedThenValidHandlesAreReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);

    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getProperties(&properties));
    EXPECT_STREQ(properties.component, "GroupName");
    EXPECT_STREQ(properties.description, "LongName");
    EXPECT_STREQ(properties.name, "SymbolName");
    EXPECT_EQ(properties.domain, 1u);
    EXPECT_EQ(properties.tierNumber, 1u);
    EXPECT_EQ(properties.samplingType, METRICS_SAMPLING_TYPE_TIME_EVENT_BASED);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenMetricProgrammableIsSupportedAndMoreThanAvailableCountIsRequestedThenSuccessIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);

    zet_metric_programmable_exp_handle_t programmable{};
    count += 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenEnableProgrammableMetricsSupportIsNotSetWhenMetricProgrammableGetIsCalledThenErrorIsReturned) {

    uint32_t count = 0;
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableProgrammableMetricsSupport.set(1);
    auto deviceContextTest = std::make_unique<MetricDeviceContext>(*device);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, deviceContextTest->metricProgrammableGet(&count, nullptr));
}

TEST_F(OaMetricProgrammableTests, givenEnumerationHasErroreWhenmetricProgrammableGetIsCalledThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    metricEnumeration->setInitializationState(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, oaMetricSource->metricProgrammableGet(&count, nullptr));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenConcurrentGroupVersionIsMetThenMetricProgrammableCountIsNonZero) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockAdapterGroup.mockParams.Version.MajorNumber = 2;
    mockAdapterGroup.mockParams.Version.MinorNumber = 0;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_GT(count, 0u);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenMetricEnumeratorIsNullThenMetricProgrammableCountIsZero) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.metricEnumeratorReturn = nullptr;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 0u);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenMetricPrototypeCountIsZeroThenMetricProgrammableCountIsZero) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.metricEnumeratorReturn->getMetricPrototypeCountReturn = 0;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 0u);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenMetricPrototypeParamsAreNullThenMetricProgrammableCountIsZero) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].getParamsReturn = nullptr;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 0u);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, whenRetrievingConcurrentGroupFromAdapterGroupThenAllParametersAreCorrect) {

    auto adapterGroupParams = metricEnumeration->getAdapterGroupParams(
        static_cast<MetricsDiscovery::IAdapterGroup_1_13 *>(&mockAdapterGroup));
    EXPECT_EQ(adapterGroupParams->Version.MajorNumber, 1u);
    EXPECT_EQ(adapterGroupParams->Version.MinorNumber, 13u);
    auto adapter = metricEnumeration->getAdapterFromAdapterGroup(
        static_cast<MetricsDiscovery::IAdapterGroup_1_13 *>(&mockAdapterGroup), 0);
    const auto adapterParams19 = metricEnumeration->getAdapterParams(adapter);
    EXPECT_EQ(adapterParams19->BusNumber, 100u);
    MetricsDiscovery::IMetricsDevice_1_13 *metricsDevice = nullptr;
    metricEnumeration->openMetricsDeviceFromAdapter(adapter, &metricsDevice);
    EXPECT_NE(metricsDevice, nullptr);
    metricsDevice = nullptr;
    metricEnumeration->openMetricsSubDeviceFromAdapter(adapter, 0, &metricsDevice);
    EXPECT_NE(metricsDevice, nullptr);
    auto concurrentGroup = metricEnumeration->getConcurrentGroupFromDevice(metricsDevice, 0);
    EXPECT_NE(concurrentGroup, nullptr);
}

TEST_F(OaMetricProgrammableTests, givenMetricProgrammableIsSupportedWhenCachingForMulitpleConcurrentGroupsThenValidHandlesAreReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 2u);
    zet_metric_programmable_exp_handle_t programmable[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, programmable));

    for (uint32_t i = 0; i < 2u; i++) {
        zet_metric_programmable_exp_properties_t properties{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable[i])->getProperties(&properties));
        EXPECT_STREQ(properties.component, "GroupName");
        EXPECT_STREQ(properties.description, "LongName");
        EXPECT_STREQ(properties.name, "SymbolName");
        EXPECT_EQ(properties.domain, 1u);
        EXPECT_EQ(properties.tierNumber, 1u);
        EXPECT_EQ(properties.samplingType, METRICS_SAMPLING_TYPE_TIME_EVENT_BASED);
    }
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenMetricsProgrammableSupportIsDisabledWhenmetricProgrammableGetIsCalledThenErrorIsReturned) {
    disableProgrammableMetricsSupport();
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 0u);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenMetricsProgrammableSupportIsDisabledWhenRetrievingConcurrentGroupFromAdapterGroupThenAllParametersAreCorrect) {

    disableProgrammableMetricsSupport();
    auto adapterGroupParams = metricEnumeration->getAdapterGroupParams(
        static_cast<MetricsDiscovery::IAdapterGroup_1_13 *>(&mockAdapterGroup));
    EXPECT_EQ(adapterGroupParams->Version.MajorNumber, 1u);
    EXPECT_EQ(adapterGroupParams->Version.MinorNumber, 13u);
    auto adapter = metricEnumeration->getAdapterFromAdapterGroup(
        static_cast<MetricsDiscovery::IAdapterGroup_1_13 *>(&mockAdapterGroup), 0);
    const auto adapterParams19 = metricEnumeration->getAdapterParams(adapter);
    EXPECT_EQ(adapterParams19->BusNumber, 100u);
    MetricsDiscovery::IMetricsDevice_1_13 *metricsDevice = nullptr;
    metricEnumeration->openMetricsDeviceFromAdapter(adapter, &metricsDevice);
    EXPECT_NE(metricsDevice, nullptr);
    metricsDevice = nullptr;
    metricEnumeration->openMetricsSubDeviceFromAdapter(adapter, 0, &metricsDevice);
    EXPECT_NE(metricsDevice, nullptr);
    auto concurrentGroup = metricEnumeration->getConcurrentGroupFromDevice(metricsDevice, 0);
    EXPECT_NE(concurrentGroup, nullptr);
}

TEST_F(OaMetricProgrammableTests, givenMetricProgrammableIsSupportedAndApiMaskIsIoStreamThenCorrectSamplingTypeIsSet) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[0].mockParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getProperties(&properties));
    EXPECT_EQ(properties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenMetricProgrammableIsSupportedAndApiMaskIsNotIoStreamThenCorrectSamplingTypeIsSet) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[0].mockParams.ApiMask = MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_OGL4_X;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getProperties(&properties));
    EXPECT_EQ(properties.samplingType, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableThenCorrectParamerterInfoIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo[4];

    uint32_t parameterCount = 6;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo));
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

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenQueryingParameterInfoWithZeroParameterCountThenReturnError) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo[4];
    uint32_t parameterCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWithZeroParametersWhenQueryingParamInfoWithZeroParamCountThenReturnSuccess) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[0].mockParams.OptionDescriptorCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo[4];
    uint32_t parameterCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenGetOptionDescriptorReturnsNullThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo[4];
    uint32_t parameterCount = 6;
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors.resize(0);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenGetOptionDescriptorTypeIsUnsupportedThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo[4];
    uint32_t parameterCount = 6;
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors[0].Type = MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_LAST;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenGetOptionDescriptorValueTypeIsUnsupportedThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo[4];
    uint32_t parameterCount = 6;
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors[0].ValidValues->ValueType = MetricsDiscovery::VALUE_TYPE_LAST;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo));

    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors[0].ValidValues->ValueType = MetricsDiscovery::VALUE_TYPE_UINT32;
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableThenCorrectParamerterValueInfoIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    std::vector<zet_metric_programmable_param_info_exp_t> parameterInfo(4);
    uint32_t parameterCount = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo.data()));
    uint32_t valueInfoCount = 2;
    zet_metric_programmable_param_value_info_exp_t valueInfo{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamValueInfo(0, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui32, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamValueInfo(1, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui64, 2u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamValueInfo(2, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Min, 3u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Max, 4u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamValueInfo(3, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Min, 5u);
    EXPECT_EQ(valueInfo.valueInfo.ui64Range.ui64Max, 6u);

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenUnsupportedValueInfoFromMdapiIsObservedThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    std::vector<zet_metric_programmable_param_info_exp_t> parameterInfo(4);
    uint32_t parameterCount = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo.data()));
    uint32_t valueInfoCount = 2;
    zet_metric_programmable_param_value_info_exp_t valueInfo{};

    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors[0].ValidValues->ValueType = MetricsDiscovery::VALUE_TYPE_LAST;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, MetricProgrammable::fromHandle(programmable)->getParamValueInfo(0, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 0u);
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors[0].ValidValues->ValueType = MetricsDiscovery::VALUE_TYPE_UINT32;
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableThenCorrectParamerterValueInfoDescIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    std::vector<zet_metric_programmable_param_info_exp_t> parameterInfo(4);
    uint32_t parameterCount = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, parameterInfo.data()));
    uint32_t valueInfoCount = 2;
    zet_metric_programmable_param_value_info_exp_t valueInfo{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamValueInfo(0, &valueInfoCount, &valueInfo));
    EXPECT_EQ(valueInfoCount, 1u);
    EXPECT_STREQ(valueInfo.description, "SymbolName");

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenQueryingForMoreParameterIndexThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    uint32_t valueInfoCount = 2;
    zet_metric_programmable_param_value_info_exp_t valueInfo{};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, MetricProgrammable::fromHandle(programmable)->getParamValueInfo(4, &valueInfoCount, &valueInfo));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenCreatingMetricsFromItThenReturnSuccess) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue[4];
    parameterValue[0].value.ui32 = 20;
    parameterValue[1].value.ui64 = 21;
    parameterValue[2].value.ui32 = 22;
    parameterValue[3].value.ui64 = 23;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(parameterValue, 4, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);

    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(parameterValue, 4, metricName, metricDescription, &metricHandleCount, &metricHandle));

    zetMetricDestroyExp(metricHandle);

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenGetOptionDescriptorReturnsNullWhenCreatingMetricsThenReturnError) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue[4];
    parameterValue[0].value.ui32 = 20;
    parameterValue[1].value.ui64 = 21;
    parameterValue[2].value.ui32 = 22;
    parameterValue[3].value.ui64 = 23;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors.resize(0);
    uint32_t metricHandleCount = 1;
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN,
              MetricProgrammable::fromHandle(programmable)->createMetric(parameterValue, 4, metricName, metricDescription, &metricHandleCount, &metricHandle));
    EXPECT_EQ(metricHandleCount, 0u);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenCreatingMetricsUsingSameValueAsDefaultValueThenReturnSuccess) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo{};
    uint32_t parameterCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->getParamInfo(&parameterCount, &parameterInfo));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui64 = parameterInfo.defaultValue.ui64;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);

    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    zetMetricDestroyExp(metricHandle);

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenCreatingMetricsWithIncorrectParametersThenReturnsFailure) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 5, metricName, metricDescription, &metricHandleCount, nullptr));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenCreatingMetricsAndMdapiFailsDuringCloneThenReturnsFailure) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 1;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].deleteClone();
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenCreatingMetricsAndMdapiFailsDuringChangeNamesThenReturnsFailure) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 1;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].clone->mockChangeNamesReturnCode = MetricsDiscovery::CC_ERROR_GENERAL;
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricProgrammableWhenCreatingMetricsWithIncorrectParametersAndMdapiFailsThenReturnsFailure) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 1;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].clone->mockSetOptionReturnCode = MetricsDiscovery::CC_ERROR_INVALID_PARAMETER;
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenAddingOrRemovingMetricsThenSuccessIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupCount, 0u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];

    ASSERT_NE(metricGroupHandle, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));

    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenInvalidMeticWhenMetricGroupIsCreatedThenErrorIsReturned) {
    MockMetricSource mockMetricSource{};
    mockMetricSource.isAvailableReturn = true;
    MockMetric mockMetric(mockMetricSource);
    uint32_t metricGroupCount = 0;
    auto metricHandle = mockMetric.toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_EQ(metricGroupCount, 0u);
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenCreateMetricGroupsFromMetricsIsCalledWhenMetricCountIsZeroThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, deviceContext->createMetricGroupsFromMetrics(0, nullptr, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_EQ(metricGroupCount, 0u);

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenDestroyingMetricsBeforeDestroyingMetricGroupThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupCount, 0u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, zetMetricDestroyExp(metricHandle));
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenFinalizingMetricsThenSuccessIsReturned) {

    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupCount, 0u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];

    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle));
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenAddingMetricsAfterFinalizingThenSuccessIsReturned) {

    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle1{}, metricHandle2{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle2));
    size_t errorStringSize = 0;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle1, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle1, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupCount, 0u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle1, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle2, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle1));
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle2));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenRemovingAllMetricsAndClosedThenErrorIsReturned) {

    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle1{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle1, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle1, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupCount, 0u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle1));
    EXPECT_EQ(ZE_RESULT_NOT_READY, zetMetricGroupCloseExp(metricGroupHandle));
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenActivatedValidMetricGroupWhenAddingOrRemovingMetricsThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));
    size_t errorStringSize = 0;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_GE(metricGroupCount, 2u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[1];

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    deviceContext->activateMetricGroupsPreferDeferred(1, &metricGroupHandle);
    deviceContext->activateMetricGroups();
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle));
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, zetMetricGroupDestroyExp(metricGroupHandle));

    deviceContext->activateMetricGroupsPreferDeferred(0, nullptr);
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenErrorDuingActivationOfMetricsTheActivationStatusIsNotChanged) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));
    size_t errorStringSize = 0;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_GE(metricGroupCount, 2u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    deviceContext->activateMetricGroupsPreferDeferred(1, &metricGroupHandle);
    deviceContext->activateMetricGroups();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandle));
    deviceContext->activateMetricGroupsPreferDeferred(0, nullptr);
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenUserDefinedMetricGroupWhenAddingMetricsAndErrorIsObservedThenErrorStringIsUpdated) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_NE(metricGroupCount, 0u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];

    zet_metric_handle_t metricHandle1{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_ERROR_GENERAL;
    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle1, &errorStringSize, nullptr));
    EXPECT_NE(errorStringSize, 0u);
    std::vector<char> errorString(errorStringSize);
    errorString[0] = '\0';
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle1, &errorStringSize, errorString.data()));
    EXPECT_NE(errorString[0], '\0');
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    metricEnumeration->cleanupExtendedMetricInformation();
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMetricProgrammableTests, givenUserDefinedMetricGroupWhenCreatingWithMetricIfErrorIsEncounteredThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_ERROR_GENERAL;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMetricProgrammableTests, givenUserDefinedMetricGroupWhenAddingMetricsWithIncompatibleSamplingTypeThenErrorIsObserved) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[0].mockParams.ApiMask = MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_OGL4_X;
    // Create Metric Handle 2 with different sampling type
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[1].mockParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    mockConcurrentGroup.mockMetricEnumerator.getMetricPrototypeCountReturn = 2;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    ASSERT_EQ(count, 2u);
    std::vector<zet_metric_programmable_exp_handle_t> programmables(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, programmables.data()));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmables[0])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle1{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[0])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle1));

    metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmables[1])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle2{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[1])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle2));

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle1, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle1, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 1u);
    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroupHandles[0], metricHandle2, &errorStringSize, nullptr));
    EXPECT_NE(errorStringSize, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle2));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenAddingOrRemovingPreDefinedMetricsThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));
    size_t errorStringSize = 0;
    zet_metric_group_handle_t metricGroupHandle = nullptr;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_GE(metricGroupCount, 2u);
    // Access the query sampling type group
    metricGroupHandle = metricGroupHandles[1];

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));

    uint32_t metricCount = 0;

    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    std::vector<zet_metric_handle_t> metricHandles(metricCount);
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, metricHandles.data()), ZE_RESULT_SUCCESS);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandles[0], &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandles[0]));

    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenAddingOrRemovingMetricsFromIncorrectSourceThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));
    size_t errorStringSize = 0;
    zet_metric_group_handle_t metricGroupHandle = nullptr;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_GE(metricGroupCount, 2u);

    metricGroupHandle = metricGroupHandles[1];

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));

    MockMetricSource mockMetricSource{};
    mockMetricSource.isAvailableReturn = true;
    MockMetric mockMetric(mockMetricSource);
    auto metricHandleIncorrectSource = mockMetric.toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandleIncorrectSource, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandleIncorrectSource));

    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenMetricSetOpenFailsWhenAddingOrRemovingMetricsThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricSet.mockApiMask = MetricsDiscovery::API_TYPE_VULKAN;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
    mockConcurrentGroup.mockMetricSet.mockOpenReturnCodeList.resize(2);
    mockConcurrentGroup.mockMetricSet.mockOpenReturnCodeList[0] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockOpenReturnCodeList[1] = MetricsDiscovery::CC_ERROR_NO_MEMORY;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo{};
    uint32_t parameterCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, &parameterInfo));
    uint32_t valueInfoCount = 1;
    zet_metric_programmable_param_value_info_exp_t paramValueInfo{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 0, &valueInfoCount, &paramValueInfo));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetPropertiesExp(programmable, &properties));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle1{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    zet_metric_handle_t metricHandle2{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, &metricHandle2));

    zet_metric_handle_t metricHandles[] = {metricHandle1, metricHandle2};

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_EQ(metricGroupCount, 2u);
    zet_metric_group_handle_t metricGroupHandle{};
    metricGroupCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, &metricGroupHandle));
    EXPECT_EQ(metricGroupCount, 1u);
    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandles[1], &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetMetricGroupRemoveMetricExp(metricGroupHandle, metricHandles[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle2));
    metricEnumeration->cleanupExtendedMetricInformation();
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenMetricGroupActivationReturnsFalseThenSuccessIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));
    size_t errorStringSize = 0;
    zet_metric_group_handle_t metricGroupHandle = nullptr;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 2u);
    metricGroupHandle = metricGroupHandles[1];

    mockConcurrentGroup.mockMetricSet.mockApiMask = MetricsDiscovery::API_TYPE_VULKAN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    deviceContext->activateMetricGroupsPreferDeferred(1, &metricGroupHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->activateMetricGroups());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[0]));
    deviceContext->activateMetricGroupsPreferDeferred(0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandles[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenDelayedActivatingMetricGroupBeforeCloseThenSuccessIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricSet.mockApiMask = MetricsDiscovery::API_TYPE_VULKAN;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo{};
    uint32_t parameterCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, &parameterInfo));
    uint32_t valueInfoCount = 1;
    zet_metric_programmable_param_value_info_exp_t paramValueInfo{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 0, &valueInfoCount, &paramValueInfo));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetPropertiesExp(programmable, &properties));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, &metricHandle));
    zet_metric_handle_t metricHandle1{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    zet_metric_group_handle_t metricGroupHandle = nullptr;

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 2u);
    metricGroupHandle = metricGroupHandles[1];

    size_t errorStringSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupAddMetricExp(metricGroupHandle, metricHandle1, &errorStringSize, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->activateMetricGroupsPreferDeferred(1, &metricGroupHandle));
    deviceContext->activateMetricGroups();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    deviceContext->activateMetricGroupsPreferDeferred(0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->activateMetricGroups());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupCloseExp(metricGroupHandle));
    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMetricProgrammableTests, givenValidProgrammableWhenCreatingProgrammableWithUnknownMdapiValueTypeThenErrorIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricSet.mockApiMask = MetricsDiscovery::API_TYPE_VULKAN;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo{};
    uint32_t parameterCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, &parameterInfo));
    uint32_t valueInfoCount = 1;
    zet_metric_programmable_param_value_info_exp_t paramValueInfo{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 0, &valueInfoCount, &paramValueInfo));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetPropertiesExp(programmable, &properties));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};

    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors[0].ValidValues->ValueType = MetricsDiscovery::VALUE_TYPE_LAST;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, &metricHandle));
    mockConcurrentGroup.metricEnumeratorReturn->metricProtoTypeReturn[0].mockOptionDescriptors[0].ValidValues->ValueType = MetricsDiscovery::VALUE_TYPE_UINT32;

    metricEnumeration->cleanupExtendedMetricInformation();
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMetricProgrammableTests, givenValidMetricGroupWhenCreatingLessThanAvailableMetricGroupsThenCorrectMetricGroupIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricSet.mockApiMask = MetricsDiscovery::API_TYPE_VULKAN;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList.resize(2);
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[1] = MetricsDiscovery::CC_ERROR_NO_MEMORY;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_info_exp_t parameterInfo{};
    uint32_t parameterCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, &parameterInfo));
    uint32_t valueInfoCount = 1;
    zet_metric_programmable_param_value_info_exp_t paramValueInfo{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetParamValueInfoExp(programmable, 0, &valueInfoCount, &paramValueInfo));
    zet_metric_programmable_exp_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricProgrammableGetPropertiesExp(programmable, &properties));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle1{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    zet_metric_handle_t metricHandle2{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricCreateFromProgrammableExp2(programmable, 1, &parameterValue, metricName, metricDescription, &metricHandleCount, &metricHandle2));

    zet_metric_handle_t metricHandles[] = {metricHandle1, metricHandle2};

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_EQ(metricGroupCount, 4u);
    zet_metric_group_handle_t metricGroupHandle{};
    metricGroupCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, &metricGroupHandle));
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandle));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle2));
    metricEnumeration->cleanupExtendedMetricInformation();
    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

TEST_F(OaMetricProgrammableTests, givenEnableProgrammableMetricsSupportIsNotSetWhenMetricGroupCreateIsCalledThenErrorIsReturned) {

    DebugManagerStateRestore restorer;
    debugManager.flags.DisableProgrammableMetricsSupport.set(1);
    auto deviceContextTest = std::make_unique<MetricDeviceContext>(*device);

    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, deviceContextTest->createMetricGroupsFromMetrics(0, nullptr, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_EQ(metricGroupCount, 0u);
}

TEST_F(OaMetricProgrammableTests, givenCreateMetricGroupsFromMetricsWhenUnembargoedMetricSourceIsUsedThenSuccessIsReturned) {
    MockMetricSource mockMetricSource{};
    mockMetricSource.isAvailableReturn = true;
    MockMetric mockMetric(mockMetricSource);
    mockMetric.setPredefined(false);

    uint32_t metricGroupCount = 0;
    auto metricHandle = mockMetric.toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(1, &metricHandle, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_EQ(metricGroupCount, 0u);

    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenCreateMetricGroupsFromMetricsWhenMultipleMetricsCouldBeAddedToMultipleGroupsThenCorrectGroupCountIsReturned) {

    MockIConcurrentGroup1x13 mockConcurrentGroup;
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[0].mockParams.ApiMask = MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_OGL4_X;
    // Create Metric Handle 2 with different sampling type
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[1].mockParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    mockConcurrentGroup.mockMetricEnumerator.getMetricPrototypeCountReturn = 2;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 2u);
    std::vector<zet_metric_programmable_exp_handle_t> programmables(2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, programmables.data()));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[0])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[1])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle1{}, metricHandle2{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[0])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[1])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle2));

    uint32_t metricGroupCount = 0;
    zet_metric_handle_t metricHandles[] = {metricHandle1, metricHandle2};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 2u);
    metricGroupHandles.resize(metricGroupCount);

    for (auto &metricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(metricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle2));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(OaMetricProgrammableTests, givenCreateMetricGroupsFromMetricsWhenFailureOccursDuringMetricGroupCreationFromMultipleMetricsThenFailureIsReturned) {

    MockIConcurrentGroup1x13 mockConcurrentGroup;

    MockIMetricSet1x13::addMetricCallCount = 0;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList.resize(4);
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[1] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[2] = MetricsDiscovery::CC_OK;
    mockConcurrentGroup.mockMetricSet.mockAddMetricReturnCodeList[3] = MetricsDiscovery::CC_ERROR_GENERAL;

    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[0].mockParams.ApiMask = MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_OGL4_X;
    // Create Metric Handle 2 with different sampling type
    mockConcurrentGroup.mockMetricEnumerator.metricProtoTypeReturn[1].mockParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM;
    mockConcurrentGroup.mockMetricEnumerator.getMetricPrototypeCountReturn = 2;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    EXPECT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    EXPECT_EQ(count, 2u);
    std::vector<zet_metric_programmable_exp_handle_t> programmables(2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, programmables.data()));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[0])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    metricHandleCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[1])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    EXPECT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle1{}, metricHandle2{}, metricHandle3{}, metricHandle4;
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[0])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[0])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[1])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle3));
    EXPECT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmables[1])->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle4));

    uint32_t metricGroupCount = 0;
    zet_metric_handle_t metricHandles[] = {metricHandle1, metricHandle2, metricHandle3, metricHandle4};
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(4, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, deviceContext->createMetricGroupsFromMetrics(4, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    EXPECT_EQ(metricGroupCount, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle3));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle4));
    metricEnumeration->cleanupExtendedMetricInformation();

    MockIMetricSet1x13::resetMocks(&mockConcurrentGroup.mockMetricSet);
}

class MultiSourceOaMetricProgrammableFixture : public DeviceFixture,
                                               public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;
    std::unique_ptr<MockMetricDeviceContextIpSampling> deviceContext = nullptr;
    OaMetricSourceImp *oaMetricSource = nullptr;
    MetricEnumeration *metricEnumeration = nullptr;
    MockIAdapterGroup1x13 mockAdapterGroup{};
    MockMetricIpSamplingSource *metricSource = nullptr;
};

void MultiSourceOaMetricProgrammableFixture::TearDown() {
    DeviceFixture::tearDown();
    deviceContext.reset();
}

void MultiSourceOaMetricProgrammableFixture::SetUp() {
    DeviceFixture::setUp();

    mockAdapterGroup.mockParams.Version.MajorNumber = 1;
    mockAdapterGroup.mockParams.Version.MinorNumber = 13;
    deviceContext = std::make_unique<MockMetricDeviceContextIpSampling>(*device);
    oaMetricSource = static_cast<OaMetricSourceImp *>(&deviceContext->getMetricSource<OaMetricSourceImp>());
    metricEnumeration = static_cast<MetricEnumeration *>(&oaMetricSource->getMetricEnumeration());
    metricEnumeration->setAdapterGroup(&mockAdapterGroup);

    oaMetricSource->setInitializationState(ZE_RESULT_SUCCESS);
    metricEnumeration->setInitializationState(ZE_RESULT_SUCCESS);

    metricSource = new MockMetricIpSamplingSource(*deviceContext);
    deviceContext->setMetricIpSamplingSource(metricSource);
}

TEST_F(MultiSourceOaMetricProgrammableFixture, givenCreateMetricGroupsFromMetricsIsCalledAndOneMetricSourcesReturnsUnsupportedThenSuccessIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    ASSERT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    ASSERT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    ASSERT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    ASSERT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    ASSERT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    MockMetricIpSamplingSource mockMetricTraceSource(*deviceContext);
    MockMetric mockMetric(mockMetricTraceSource);
    mockMetric.setPredefined(false);

    zet_metric_handle_t metricHandles[] = {metricHandle, mockMetric.toHandle()};

    uint32_t metricGroupCount = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    ASSERT_NE(metricGroupCount, 0u);
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    ASSERT_NE(metricGroupCount, 0u);
    zet_metric_group_handle_t metricGroupHandle = metricGroupHandles[0];

    ASSERT_NE(metricGroupHandle, nullptr);

    for (auto destroyMetricGroupHandle : metricGroupHandles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGroupDestroyExp(destroyMetricGroupHandle));
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(MultiSourceOaMetricProgrammableFixture, givenCreateMetricGroupsFromMetricsIsCalledAndOneMetricSourcesFailsThenFailureIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    ASSERT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    ASSERT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    ASSERT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    ASSERT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    ASSERT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    MockMetricIpSamplingSource &mockMetricTraceSource = static_cast<MockMetricIpSamplingSource &>(deviceContext->getMetricSource<IpSamplingMetricSourceImp>());
    MockMetric mockMetric(mockMetricTraceSource);
    mockMetric.setPredefined(false);

    zet_metric_handle_t metricHandles[] = {metricHandle, mockMetric.toHandle()};
    uint32_t metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    ASSERT_NE(metricGroupCount, 0u);
    // Allocate more space for the mockMetricTraceSource
    metricGroupCount += 1;
    std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
    mockMetricTraceSource.createMetricGroupsFromMetricsReturn = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, metricGroupHandles.data()));
    ASSERT_EQ(metricGroupCount, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(MultiSourceOaMetricProgrammableFixture, givenMetricProgrammableGetReturnsFailureForOneOfTheSourcesThenApiReturnsFailure) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    ASSERT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    ASSERT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    MockMetricIpSamplingSource &mockMetricTraceSource = static_cast<MockMetricIpSamplingSource &>(deviceContext->getMetricSource<IpSamplingMetricSourceImp>());
    mockMetricTraceSource.metricProgrammableGetReturn = ZE_RESULT_ERROR_UNKNOWN;
    count += 1;
    ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, deviceContext->metricProgrammableGet(&count, &programmable));
    metricEnumeration->cleanupExtendedMetricInformation();
}

TEST_F(MultiSourceOaMetricProgrammableFixture, givenCreateMetricGroupsFromMetricsIsCalledAndOneMetricSourcesFailsDuringCountCalculationThenFailureIsReturned) {
    MockIConcurrentGroup1x13 mockConcurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup1x13 = mockConcurrentGroup;
    ASSERT_EQ(ZE_RESULT_SUCCESS, metricEnumeration->cacheExtendedMetricInformation(concurrentGroup1x13, 1));
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, nullptr));
    ASSERT_EQ(count, 1u);
    zet_metric_programmable_exp_handle_t programmable{};
    ASSERT_EQ(ZE_RESULT_SUCCESS, deviceContext->metricProgrammableGet(&count, &programmable));
    zet_metric_programmable_param_value_exp_t parameterValue{};
    parameterValue.value.ui32 = 20;
    uint32_t metricHandleCount = 0;
    const char metricName[ZET_MAX_METRIC_NAME] = "metricName";
    const char metricDescription[ZET_MAX_METRIC_NAME] = "metricDescription";
    ASSERT_EQ(ZE_RESULT_SUCCESS,
              MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, nullptr));
    ASSERT_EQ(metricHandleCount, 1u);
    zet_metric_handle_t metricHandle{};
    ASSERT_EQ(ZE_RESULT_SUCCESS, MetricProgrammable::fromHandle(programmable)->createMetric(&parameterValue, 1, metricName, metricDescription, &metricHandleCount, &metricHandle));

    MockMetricIpSamplingSource &mockMetricTraceSource = static_cast<MockMetricIpSamplingSource &>(deviceContext->getMetricSource<IpSamplingMetricSourceImp>());
    MockMetric mockMetric(mockMetricTraceSource);
    mockMetric.setPredefined(false);

    zet_metric_handle_t metricHandles[] = {metricHandle, mockMetric.toHandle()};
    uint32_t metricGroupCount = 0;
    mockMetricTraceSource.createMetricGroupsFromMetricsReturn = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, deviceContext->createMetricGroupsFromMetrics(2, metricHandles, "metricGroupName", "metricGroupDesc", &metricGroupCount, nullptr));
    EXPECT_EQ(metricGroupCount, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricDestroyExp(metricHandle));
    metricEnumeration->cleanupExtendedMetricInformation();
    mockMetricTraceSource.createMetricGroupsFromMetricsReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace ult
} // namespace L0
