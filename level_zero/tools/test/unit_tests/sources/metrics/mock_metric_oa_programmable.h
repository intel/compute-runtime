/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/metrics/metric_oa_programmable_imp.h"

namespace L0 {
namespace ult {
class MockIMetricPrototype1x13 : public MetricsDiscovery::IMetricPrototype_1_13 {
  public:
    MetricsDiscovery::TMetricPrototypeParams_1_13 mockParams{};
    MetricsDiscovery::TMetricPrototypeParams_1_13 *getParamsReturn = &mockParams;
    std::vector<MetricsDiscovery::TMetricPrototypeOptionDescriptor_1_13> mockOptionDescriptors{};
    MetricsDiscovery::TCompletionCode mockSetOptionReturnCode = MetricsDiscovery::CC_OK;
    MockIMetricPrototype1x13 *clone = nullptr;
    MetricsDiscovery::TCompletionCode mockChangeNamesReturnCode = MetricsDiscovery::CC_OK;
    void setUpDefaultParams(bool createClone);
    void deleteClone();

    ~MockIMetricPrototype1x13() override;
    const MetricsDiscovery::TMetricPrototypeParams_1_13 *GetParams(void) const override;
    MetricsDiscovery::IMetricPrototype_1_13 *Clone(void) override;
    const MetricsDiscovery::TMetricPrototypeOptionDescriptor_1_13 *GetOptionDescriptor(uint32_t index) const override;
    MetricsDiscovery::TCompletionCode SetOption(const MetricsDiscovery::TOptionDescriptorType optionType, const MetricsDiscovery::TTypedValue_1_0 *typedValue) override;
    MetricsDiscovery::TCompletionCode ChangeNames(const char *symbolName, const char *shortName, const char *longName, const char *resultUnits) override;
};

constexpr uint32_t maxMockPrototypesSupported = 2;

class MockIMetricEnumerator1x13 : public MetricsDiscovery::IMetricEnumerator_1_13 {
  public:
    ~MockIMetricEnumerator1x13() override = default;
    uint32_t getMetricPrototypeCountReturn = 1;
    uint32_t GetMetricPrototypeCount(void) override;
    MetricsDiscovery::IMetricPrototype_1_13 *GetMetricPrototype(const uint32_t index) override;
    MockIMetricPrototype1x13 metricProtoTypeReturn[maxMockPrototypesSupported];
    MetricsDiscovery::TCompletionCode GetMetricPrototypes(const uint32_t index, uint32_t *count,
                                                          MetricsDiscovery::IMetricPrototype_1_13 **metrics) override;

    MockIMetricEnumerator1x13();
};

class MockIMetric1x13 : public MetricsDiscovery::IMetric_1_13 {
  public:
    MetricsDiscovery::TMetricParams_1_13 mockMetricParams{};
    ~MockIMetric1x13() override{};
    MetricsDiscovery::TMetricParams_1_13 *GetParams() override;
};

class MockIInformation1x0 : public MetricsDiscovery::IInformation_1_0 {
  public:
    MetricsDiscovery::TInformationParams_1_0 mockInformationParams{};
    ~MockIInformation1x0() override{};
    MetricsDiscovery::TInformationParams_1_0 *GetParams() override;
};

class MockIMetricSet1x13 : public MetricsDiscovery::IMetricSet_1_13 {
  public:
    MetricsDiscovery::TMetricSetParams_1_11 mockMetricSetParams{};
    std::vector<MetricsDiscovery::TCompletionCode> mockOpenReturnCodeList{MetricsDiscovery::CC_OK};
    std::vector<MetricsDiscovery::TCompletionCode> mockAddMetricReturnCodeList{MetricsDiscovery::CC_OK};
    std::vector<MetricsDiscovery::TCompletionCode> mockRemoveMetricReturnCodeList{MetricsDiscovery::CC_OK};
    std::vector<MetricsDiscovery::TCompletionCode> mockCloseReturnCodeList{MetricsDiscovery::CC_OK};
    uint32_t mockApiMask = MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_IOSTREAM;
    std::vector<uint32_t> mockMetricCountList{2u};
    uint32_t mockInformationCount = 1;
    MockIInformation1x0 mockInformation{};
    static uint32_t openMetricCallCount;
    static uint32_t addMetricCallCount;
    static uint32_t removeMetricCallCount;
    static uint32_t closeCallCount;
    static uint32_t getParamsCallCount;
    MockIMetric1x13 mockMetric{};
    ~MockIMetricSet1x13() override {}
    MetricsDiscovery::TCompletionCode Open() override;
    MetricsDiscovery::TCompletionCode AddMetric(MetricsDiscovery::IMetricPrototype_1_13 *metricPrototype) override;
    MetricsDiscovery::TCompletionCode RemoveMetric(MetricsDiscovery::IMetricPrototype_1_13 *metricPrototype) override;
    MetricsDiscovery::TCompletionCode Finalize() override;
    MetricsDiscovery::TMetricSetParams_1_11 *GetParams(void) override;
    MetricsDiscovery::TCompletionCode SetApiFiltering(uint32_t apiMask) override;

    MetricsDiscovery::IMetric_1_13 *GetMetric(uint32_t index) override;
    MetricsDiscovery::IInformation_1_0 *GetInformation(uint32_t index) override;
    static void resetMocks(MockIMetricSet1x13 *mockMetricSet);
};

class MockIConcurrentGroup1x13 : public MetricsDiscovery::IConcurrentGroup_1_13 {
  public:
    ~MockIConcurrentGroup1x13() override {}
    MockIMetricEnumerator1x13 mockMetricEnumerator;
    MockIMetricSet1x13 mockMetricSet;

    MockIMetricEnumerator1x13 *metricEnumeratorReturn = &mockMetricEnumerator;
    MetricsDiscovery::TCompletionCode mockRemoveMetricSetReturn = MetricsDiscovery::CC_OK;
    MetricsDiscovery::IMetricEnumerator_1_13 *GetMetricEnumerator() override;
    MetricsDiscovery::TConcurrentGroupParams_1_13 *GetParams(void) override;
    MetricsDiscovery::IMetricSet_1_13 *GetMetricSet(uint32_t index) override;
    MetricsDiscovery::IMetricEnumerator_1_13 *GetMetricEnumeratorFromFile(const char *fileName) override;
    MetricsDiscovery::IMetricSet_1_13 *AddMetricSet(const char *symbolName, const char *shortName) override;
    MetricsDiscovery::TCompletionCode RemoveMetricSet(MetricsDiscovery::IMetricSet_1_13 *metricSet) override;
};

class MockIMetricsDevice1x13 : public MetricsDiscovery::IMetricsDevice_1_13 {
  public:
    MockIConcurrentGroup1x13 concurrentGroup{};
    MockIConcurrentGroup1x13 *concurrentGroupReturn = &concurrentGroup;
    MetricsDiscovery::IConcurrentGroup_1_13 *GetConcurrentGroup(uint32_t index) override;
};

class MockIAdapter1x13 : public MetricsDiscovery::IAdapter_1_13 {
  public:
    MockIMetricsDevice1x13 mockMetricsDevice{};
    MockIMetricsDevice1x13 *metricsDeviceReturn = &mockMetricsDevice;
    MetricsDiscovery::TCompletionCode OpenMetricsDevice(MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) override;
    MetricsDiscovery::TCompletionCode OpenMetricsDevice(MetricsDiscovery::IMetricsDevice_1_5 **metricsDevice) override;
    MetricsDiscovery::TCompletionCode OpenMetricsDeviceFromFile(const char *fileName, void *openParams, MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) override;
    MetricsDiscovery::TCompletionCode OpenMetricsSubDevice(const uint32_t subDeviceIndex, MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) override;
    MetricsDiscovery::TCompletionCode OpenMetricsSubDevice(const uint32_t subDeviceIndex, MetricsDiscovery::IMetricsDevice_1_5 **metricsDevice) override;
    MetricsDiscovery::TCompletionCode OpenMetricsSubDeviceFromFile(const uint32_t subDeviceIndex, const char *fileName, void *openParams, MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) override;
    MetricsDiscovery::TAdapterParams_1_9 mockAdapterParams;
    const MetricsDiscovery::TAdapterParams_1_9 *GetParams(void) const override;
    MockIAdapter1x13();
};
class MockIAdapterGroup1x13 : public MetricsDiscovery::IAdapterGroup_1_13 {
  public:
    ~MockIAdapterGroup1x13() override = default;
    MetricsDiscovery::TAdapterGroupParams_1_6 mockParams{};
    const MetricsDiscovery::TAdapterGroupParams_1_6 *GetParams() const override;
    MockIAdapter1x13 mockAdapter{};
    MockIAdapter1x13 *adapterReturn = &mockAdapter;
    MetricsDiscovery::IAdapter_1_13 *GetAdapter(uint32_t index) override;
    MetricsDiscovery::TCompletionCode Close() override;
};

} // namespace ult
} // namespace L0