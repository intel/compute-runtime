/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa_programmable.h"

#include "shared/test/common/test_macros/test.h"

namespace L0 {
namespace ult {

uint32_t MockIMetricSet1x13::openMetricCallCount = 0;
uint32_t MockIMetricSet1x13::addMetricCallCount = 0;
uint32_t MockIMetricSet1x13::removeMetricCallCount = 0;
uint32_t MockIMetricSet1x13::getParamsCallCount = 0;
uint32_t MockIMetricSet1x13::closeCallCount = 0;

const MetricsDiscovery::TMetricPrototypeParams_1_13 *MockIMetricPrototype1x13::GetParams(void) const {
    return getParamsReturn;
}
void MockIMetricPrototype1x13::setUpDefaultParams(bool createClone) {
    mockParams.GroupName = "GroupName";
    mockParams.LongName = "LongName";
    mockParams.SymbolName = "SymbolName";
    mockParams.UsageFlagsMask = MetricsDiscovery::USAGE_FLAG_TIER_1;
    mockParams.ApiMask = MetricsDiscovery::API_TYPE_IOSTREAM |
                         MetricsDiscovery::API_TYPE_OCL |
                         MetricsDiscovery::API_TYPE_OGL4_X;
    mockParams.OptionDescriptorCount = 4;
    mockParams.MetricType = MetricsDiscovery::METRIC_TYPE_EVENT;
    mockParams.MetricResultUnits = "percent";
    mockParams.ResultType = MetricsDiscovery::RESULT_UINT64;
    mockParams.HwUnitType = MetricsDiscovery::HW_UNIT_GPU;
    mockParams.ShortName = "ShortName";

    // Setup 4 option descriptors
    {
        MetricsDiscovery::TMetricPrototypeOptionDescriptor_1_13 mockOptionDescriptor;
        mockOptionDescriptor.SymbolName = "mockParam1";
        mockOptionDescriptor.Type = MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_DISAGGREGATION;
        mockOptionDescriptor.ValidValueCount = 1;
        static MetricsDiscovery::TValidValue_1_13 validValue{};
        validValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT32;
        validValue.ValueUInt32 = 1;
        mockOptionDescriptor.ValidValues = &validValue;
        mockOptionDescriptors.push_back(mockOptionDescriptor);
    }

    {
        MetricsDiscovery::TMetricPrototypeOptionDescriptor_1_13 mockOptionDescriptor;
        mockOptionDescriptor.SymbolName = "mockParam2";
        mockOptionDescriptor.Type = MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_LATENCY;
        mockOptionDescriptor.ValidValueCount = 1;
        static MetricsDiscovery::TValidValue_1_13 validValue{};
        validValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT64;
        validValue.ValueUInt64 = 2;
        mockOptionDescriptor.ValidValues = &validValue;
        mockOptionDescriptors.push_back(mockOptionDescriptor);
    }

    {
        MetricsDiscovery::TMetricPrototypeOptionDescriptor_1_13 mockOptionDescriptor;
        mockOptionDescriptor.SymbolName = "mockParam3";
        mockOptionDescriptor.Type = MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_UTILIZATION;
        mockOptionDescriptor.ValidValueCount = 1;
        static MetricsDiscovery::TValidValue_1_13 validValue{};
        validValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT32_RANGE;
        validValue.ValueUInt32Range.Min = 3;
        validValue.ValueUInt32Range.Max = 4;
        mockOptionDescriptor.ValidValues = &validValue;
        mockOptionDescriptors.push_back(mockOptionDescriptor);
    }

    {
        MetricsDiscovery::TMetricPrototypeOptionDescriptor_1_13 mockOptionDescriptor;
        mockOptionDescriptor.SymbolName = "mockParam4";
        mockOptionDescriptor.Type = MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_BYTE;
        mockOptionDescriptor.ValidValueCount = 1;
        static MetricsDiscovery::TValidValue_1_13 validValue{};
        validValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT64_RANGE;
        validValue.ValueUInt64Range.Min = 5;
        validValue.ValueUInt64Range.Max = 6;
        mockOptionDescriptor.ValidValues = &validValue;
        mockOptionDescriptors.push_back(mockOptionDescriptor);
    }

    if (!clone && createClone) {
        clone = new MockIMetricPrototype1x13();
        clone->setUpDefaultParams(false);
    }
}

void MockIMetricPrototype1x13::deleteClone() {
    if (clone) {
        delete clone;
        clone = nullptr;
    }
}

MockIMetricPrototype1x13::~MockIMetricPrototype1x13() {
    deleteClone();
}

MetricsDiscovery::IMetricPrototype_1_13 *MockIMetricPrototype1x13::Clone(void) {
    return clone;
}
const MetricsDiscovery::TMetricPrototypeOptionDescriptor_1_13 *MockIMetricPrototype1x13::GetOptionDescriptor(uint32_t index) const {
    if (index < mockOptionDescriptors.size()) {
        return &mockOptionDescriptors[index];
    } else {
        return nullptr;
    }
}

MetricsDiscovery::TCompletionCode MockIMetricPrototype1x13::SetOption(const MetricsDiscovery::TOptionDescriptorType optionType, const MetricsDiscovery::TTypedValue_1_0 *typedValue) {
    return mockSetOptionReturnCode;
}
MetricsDiscovery::TCompletionCode MockIMetricPrototype1x13::ChangeNames(const char *symbolName, const char *shortName, const char *longName, const char *resultUnits) {
    mockParams.LongName = longName;
    mockParams.SymbolName = symbolName;
    return mockChangeNamesReturnCode;
}

uint32_t MockIMetricEnumerator1x13::GetMetricPrototypeCount(void) {
    return getMetricPrototypeCountReturn;
}

MetricsDiscovery::IMetricPrototype_1_13 *MockIMetricEnumerator1x13::GetMetricPrototype(const uint32_t index) {
    return nullptr;
}

MetricsDiscovery::TCompletionCode MockIMetricEnumerator1x13::GetMetricPrototypes(
    const uint32_t index, uint32_t *count,
    MetricsDiscovery::IMetricPrototype_1_13 **metrics) {
    *count = getMetricPrototypeCountReturn;

    for (uint32_t index = 0; index < *count; index++) {
        metrics[index] = &metricProtoTypeReturn[index % maxMockPrototypesSupported];
    }
    return MetricsDiscovery::CC_OK;
}

MockIMetricEnumerator1x13::MockIMetricEnumerator1x13() {
    for (uint32_t index = 0; index < maxMockPrototypesSupported; index++) {
        metricProtoTypeReturn[index].setUpDefaultParams(true);
    }
}

MetricsDiscovery::TMetricParams_1_13 *MockIMetric1x13::GetParams() {
    mockMetricParams.SymbolName = "Metric symbol name";
    mockMetricParams.ShortName = "Metric short name";
    mockMetricParams.LongName = "Metric long name";
    mockMetricParams.ResultType = MetricsDiscovery::TMetricResultType::RESULT_UINT64;
    mockMetricParams.MetricType = MetricsDiscovery::TMetricType::METRIC_TYPE_RATIO;

    return &mockMetricParams;
}

MetricsDiscovery::TInformationParams_1_0 *MockIInformation1x0::GetParams() {
    mockInformationParams.SymbolName = "Info symbol name";
    mockInformationParams.ShortName = "Info short name";
    mockInformationParams.LongName = "Info long name";
    mockInformationParams.InfoType = MetricsDiscovery::INFORMATION_TYPE_REPORT_REASON;

    return &mockInformationParams;
}

MetricsDiscovery::TCompletionCode MockIMetricSet1x13::Open() {
    auto index = std::min<uint32_t>(openMetricCallCount, static_cast<uint32_t>(mockOpenReturnCodeList.size() - 1));
    auto returnStatus = mockOpenReturnCodeList[index];
    openMetricCallCount++;
    return returnStatus;
}

MetricsDiscovery::TCompletionCode MockIMetricSet1x13::AddMetric(MetricsDiscovery::IMetricPrototype_1_13 *metricPrototype) {

    auto index = std::min<uint32_t>(addMetricCallCount, static_cast<uint32_t>(mockAddMetricReturnCodeList.size() - 1));
    auto returnStatus = mockAddMetricReturnCodeList[index];
    addMetricCallCount++;
    return returnStatus;
}

MetricsDiscovery::TCompletionCode MockIMetricSet1x13::RemoveMetric(MetricsDiscovery::IMetricPrototype_1_13 *metricPrototype) {
    auto index = std::min<uint32_t>(removeMetricCallCount, static_cast<uint32_t>(mockRemoveMetricReturnCodeList.size() - 1));
    auto returnStatus = mockRemoveMetricReturnCodeList[index];
    removeMetricCallCount++;
    return returnStatus;
}
MetricsDiscovery::TCompletionCode MockIMetricSet1x13::Finalize() {
    auto index = std::min<uint32_t>(closeCallCount, static_cast<uint32_t>(mockCloseReturnCodeList.size() - 1));
    auto returnStatus = mockCloseReturnCodeList[index];
    closeCallCount++;
    return returnStatus;
}

MetricsDiscovery::TMetricSetParams_1_11 *MockIMetricSet1x13::GetParams(void) {
    mockMetricSetParams.ApiMask = mockApiMask;
    mockMetricSetParams.InformationCount = mockInformationCount;
    auto index = std::min<uint32_t>(getParamsCallCount, static_cast<uint32_t>(mockMetricCountList.size() - 1));
    mockMetricSetParams.MetricsCount = mockMetricCountList[index];
    getParamsCallCount++;
    mockMetricSetParams.SymbolName = "Metric set name";
    mockMetricSetParams.ShortName = "Metric set description";
    return &mockMetricSetParams;
}

MetricsDiscovery::TCompletionCode MockIMetricSet1x13::SetApiFiltering(uint32_t apiMask) { return MetricsDiscovery::CC_OK; }

MetricsDiscovery::IMetric_1_13 *MockIMetricSet1x13::GetMetric(uint32_t index) {
    return &mockMetric;
}

MetricsDiscovery::IInformation_1_0 *MockIMetricSet1x13::GetInformation(uint32_t index) {
    return &mockInformation;
}

void MockIMetricSet1x13::resetMocks(MockIMetricSet1x13 *mockMetricSet) {
    mockMetricSet->mockAddMetricReturnCodeList.resize(1);
    mockMetricSet->mockAddMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    addMetricCallCount = 0;

    mockMetricSet->mockRemoveMetricReturnCodeList.resize(1);
    mockMetricSet->mockRemoveMetricReturnCodeList[0] = MetricsDiscovery::CC_OK;
    removeMetricCallCount = 0;

    mockMetricSet->mockOpenReturnCodeList.resize(1);
    mockMetricSet->mockOpenReturnCodeList[0] = MetricsDiscovery::CC_OK;
    openMetricCallCount = 0;

    mockMetricSet->mockCloseReturnCodeList.resize(1);
    mockMetricSet->mockCloseReturnCodeList[0] = MetricsDiscovery::CC_OK;
    closeCallCount = 0;

    mockMetricSet->mockMetricCountList.resize(1);
    mockMetricSet->mockMetricCountList[0] = 2;
    getParamsCallCount = 0;

    mockMetricSet->mockInformationCount = 1;
    mockMetricSet->mockApiMask = MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_IOSTREAM;
}

MetricsDiscovery::IMetricEnumerator_1_13 *MockIConcurrentGroup1x13::GetMetricEnumerator() {
    return metricEnumeratorReturn;
}

MetricsDiscovery::TConcurrentGroupParams_1_13 *MockIConcurrentGroup1x13::GetParams(void) { return nullptr; }
MetricsDiscovery::IMetricSet_1_13 *MockIConcurrentGroup1x13::GetMetricSet(uint32_t index) { return nullptr; }
MetricsDiscovery::IMetricEnumerator_1_13 *MockIConcurrentGroup1x13::GetMetricEnumeratorFromFile(const char *fileName) { return nullptr; }
MetricsDiscovery::IMetricSet_1_13 *MockIConcurrentGroup1x13::AddMetricSet(const char *symbolName, const char *shortName) { return &mockMetricSet; }
MetricsDiscovery::TCompletionCode MockIConcurrentGroup1x13::RemoveMetricSet(MetricsDiscovery::IMetricSet_1_13 *metricSet) { return mockRemoveMetricSetReturn; }

MetricsDiscovery::IConcurrentGroup_1_13 *MockIMetricsDevice1x13::GetConcurrentGroup(uint32_t index) {
    return concurrentGroupReturn;
}

MetricsDiscovery::TCompletionCode MockIAdapter1x13::OpenMetricsDevice(MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) {
    *metricsDevice = metricsDeviceReturn;
    return MetricsDiscovery::CC_OK;
}
MetricsDiscovery::TCompletionCode MockIAdapter1x13::OpenMetricsDevice(MetricsDiscovery::IMetricsDevice_1_5 **metricsDevice) {
    *metricsDevice = static_cast<MetricsDiscovery::IMetricsDevice_1_5 *>(metricsDeviceReturn);
    return MetricsDiscovery::CC_OK;
}
MetricsDiscovery::TCompletionCode MockIAdapter1x13::OpenMetricsDeviceFromFile(const char *fileName, void *openParams, MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) {
    return MetricsDiscovery::CC_ERROR_NOT_SUPPORTED;
}
MetricsDiscovery::TCompletionCode MockIAdapter1x13::OpenMetricsSubDevice(const uint32_t subDeviceIndex, MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) {
    *metricsDevice = metricsDeviceReturn;
    return MetricsDiscovery::CC_OK;
}
MetricsDiscovery::TCompletionCode MockIAdapter1x13::OpenMetricsSubDevice(const uint32_t subDeviceIndex, MetricsDiscovery::IMetricsDevice_1_5 **metricsDevice) {
    *metricsDevice = static_cast<MetricsDiscovery::IMetricsDevice_1_5 *>(metricsDeviceReturn);
    return MetricsDiscovery::CC_OK;
}
MetricsDiscovery::TCompletionCode MockIAdapter1x13::OpenMetricsSubDeviceFromFile(const uint32_t subDeviceIndex, const char *fileName, void *openParams, MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) {
    return MetricsDiscovery::CC_ERROR_NOT_SUPPORTED;
}

const MetricsDiscovery::TAdapterParams_1_9 *MockIAdapter1x13::GetParams(void) const {
    return &mockAdapterParams;
}
MockIAdapter1x13::MockIAdapter1x13() {
    mockAdapterParams.BusNumber = 100;
}

const MetricsDiscovery::TAdapterGroupParams_1_6 *MockIAdapterGroup1x13::GetParams() const {
    return &mockParams;
}
MetricsDiscovery::IAdapter_1_13 *MockIAdapterGroup1x13::GetAdapter(uint32_t index) {
    return adapterReturn;
}
MetricsDiscovery::TCompletionCode MockIAdapterGroup1x13::Close() {
    return MetricsDiscovery::CC_OK;
}

} // namespace ult
} // namespace L0