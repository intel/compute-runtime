/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/include/level_zero/zet_intel_gpu_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include "level_zero/zet_api.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

TEST(MultidevMetric, GivenMultideviceMetricCreatedThenReferenceIsUpdatedSuccessfully) {
    MockMetricSource metricSource{};
    MockMetric mockMetric(metricSource);

    std::vector<MetricImp *> subDeviceMetrics;
    subDeviceMetrics.push_back(&mockMetric);

    MultiDeviceMetricImp *multiDevMetric = MultiDeviceMetricImp::create(metricSource, subDeviceMetrics);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, multiDevMetric->destroy());

    zet_metric_properties_t properties;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, multiDevMetric->getProperties(&properties));

    MetricImp *metric = multiDevMetric->getMetricAtSubDeviceIndex(0);
    EXPECT_EQ(metric, &mockMetric);
    MultiDeviceMetricImp *multiDevMetricGet = mockMetric.getRootDevMetric();
    EXPECT_EQ(multiDevMetricGet, multiDevMetric);

    // asking for an index beyond the size of submetrics array should be handled
    metric = multiDevMetric->getMetricAtSubDeviceIndex(2);
    EXPECT_EQ(metric, nullptr);

    delete multiDevMetric;
}

class CalcOperationFixture : public DeviceFixture,
                             public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;
    MockMetricSource mockMetricSource{};
    MockMetricGroup *mockMetricGroup;
    zet_metric_group_handle_t phMetricGroup = nullptr;

    DebugManagerStateRestore restorer;
};

void CalcOperationFixture::SetUp() {
    DeviceFixture::setUp();
    mockMetricGroup = new MockMetricGroup(mockMetricSource);
    phMetricGroup = mockMetricGroup->toHandle();
}

void CalcOperationFixture::TearDown() {
    DeviceFixture::tearDown();
    delete mockMetricGroup;
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpAndInvalidParamsPassedThenErrorIsHandled) {

    // Aggregation window is zero
    zet_intel_metric_calculate_exp_desc_t calculateDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP,
        nullptr,        // pNext
        1,              // metricGroupCount
        &phMetricGroup, // phMetricGroups
        0,              // metricCount
        nullptr,        // phMetrics
        0,              // timeWindowsCount
        nullptr,        // pCalculateTimeWindows
        0,              // timeAggregationWindow, zero is not accepted
    };

    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateOperationCreateExp(context->toHandle(),
                                                                                          device->toHandle(), &calculateDesc,
                                                                                          &hCalculateOperation));

    // No metric groups or metrics
    calculateDesc.timeAggregationWindow = 100;
    calculateDesc.metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateOperationCreateExp(context->toHandle(),
                                                                                          device->toHandle(), &calculateDesc,
                                                                                          &hCalculateOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpWithMixedSourcesThenErrorIsReturned) {

    MockMetricSource mockMetricSource2{};
    mockMetricSource2.setType(UINT32_MAX);
    MockMetricGroup mockMetricGroup2(mockMetricSource2);

    // metric groups from different source
    std::vector<zet_metric_group_handle_t> metricGroups{phMetricGroup, mockMetricGroup2.toHandle()};
    zet_intel_metric_calculate_exp_desc_t calculateDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP,
        nullptr,             // pNext
        2,                   // metricGroupCount
        metricGroups.data(), // phMetricGroups
        0,                   // metricCount
        nullptr,             // phMetrics
        0,                   // timeWindowsCount
        nullptr,             // pCalculateTimeWindows
        1000,                // timeAggregationWindow
    };

    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateOperationCreateExp(context,
                                                                                          device->toHandle(), &calculateDesc,
                                                                                          &hCalculateOperation));

    // metrics from different source
    MockMetric mockMetric(mockMetricSource);
    MockMetric mockMetric2(mockMetricSource2);
    std::vector<zet_metric_handle_t> metrics{mockMetric.toHandle(), mockMetric2.toHandle()};

    calculateDesc.metricGroupCount = 1;
    calculateDesc.phMetricGroups = &phMetricGroup;
    calculateDesc.metricCount = 2;
    calculateDesc.phMetrics = metrics.data();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateOperationCreateExp(context,
                                                                                          device->toHandle(), &calculateDesc,
                                                                                          &hCalculateOperation));

    // metrics and metric group from different source
    calculateDesc.metricCount = 1;
    calculateDesc.phMetrics = &metrics[1];
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateOperationCreateExp(context,
                                                                                          device->toHandle(), &calculateDesc,
                                                                                          &hCalculateOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpWithMixedHierarchiesThenErrorIsReturned) {
    MockMetricGroup mockMetricGroup2(mockMetricSource);
    mockMetricGroup2.isMultiDevice = true;

    // metric groups from different hierarchy
    std::vector<zet_metric_group_handle_t> metricGroups{phMetricGroup, mockMetricGroup2.toHandle()};
    zet_intel_metric_calculate_exp_desc_t calculateDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP,
        nullptr,             // pNext
        2,                   // metricGroupCount
        metricGroups.data(), // phMetricGroups
        0,                   // metricCount
        nullptr,             // phMetrics
        0,                   // timeWindowsCount
        nullptr,             // pCalculateTimeWindows
        1000,                // timeAggregationWindow
    };

    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateOperationCreateExp(context,
                                                                                          device->toHandle(), &calculateDesc,
                                                                                          &hCalculateOperation));

    MockMetric mockMetric(mockMetricSource), mockMetric2(mockMetricSource);
    mockMetric2.setMultiDevice(true);

    std::vector<zet_metric_handle_t> metrics{mockMetric.toHandle(), mockMetric2.toHandle()};

    calculateDesc.metricGroupCount = 1;
    calculateDesc.phMetricGroups = &phMetricGroup;
    calculateDesc.metricCount = 2;
    calculateDesc.phMetrics = metrics.data();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateOperationCreateExp(context,
                                                                                          device->toHandle(), &calculateDesc,
                                                                                          &hCalculateOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpUseTheSourceFromMetricGroupOrMetricWhenAvailable) {

    MockMetricGroup mockMetricGroup2(mockMetricSource);

    std::vector<zet_metric_group_handle_t> metricGroups{phMetricGroup, mockMetricGroup2.toHandle()};
    zet_intel_metric_calculate_exp_desc_t calculateDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP,
        nullptr,             // pNext
        2,                   // metricGroupCount
        metricGroups.data(), // phMetricGroups
        0,                   // metricCount
        nullptr,             // phMetrics
        0,                   // timeWindowsCount
        nullptr,             // pCalculateTimeWindows
        1000,                // timeAggregationWindow
    };

    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricCalculateOperationCreateExp(context,
                                                                                             device->toHandle(), &calculateDesc,
                                                                                             &hCalculateOperation));

    MockMetric mockMetric(mockMetricSource), mockMetric2(mockMetricSource);
    std::vector<zet_metric_handle_t> metrics{mockMetric.toHandle(), mockMetric.toHandle()};

    calculateDesc.metricGroupCount = 0;
    calculateDesc.phMetricGroups = nullptr;
    calculateDesc.metricCount = 2;
    calculateDesc.phMetrics = metrics.data();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricCalculateOperationCreateExp(context,
                                                                                             device->toHandle(), &calculateDesc,
                                                                                             &hCalculateOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpObjectToAndFromHandleBaseClassWorkAsExpected) {
    std::vector<MetricImp *> mockMetricsInReport{};
    uint32_t mockExcludedMetricCount = 0;
    std::vector<MetricImp *> mockExcludedMetrics{};

    MockMetricCalcOp mockMetricCalcOp(false, mockMetricsInReport, mockExcludedMetricCount, mockExcludedMetrics);
    auto hMockCalcOp = mockMetricCalcOp.toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricCalculateOperationDestroyExp(hMockCalcOp));
    EXPECT_EQ(false, mockMetricCalcOp.isRootDevice());
    auto mockCalcOp = MetricCalcOp::fromHandle(hMockCalcOp);
    EXPECT_NE(nullptr, mockCalcOp);
}

TEST_F(CalcOperationFixture, WhenGettingMetricsInReportAndExcludedMetricsThenTheyAreReturnedCorrectly) {
    std::vector<MetricImp *> mockMetricsInReport{};
    MockMetric mockMetricInReport(mockMetricSource);
    mockMetricsInReport.push_back(&mockMetricInReport);

    std::vector<MetricImp *> mockExcludedMetrics{};
    uint32_t mockExcludedMetricCount = 1;
    MockMetric mockExcludedMetric(mockMetricSource);
    mockExcludedMetrics.push_back(&mockExcludedMetric);

    MockMetricCalcOp mockMetricCalcOp(false, mockMetricsInReport, mockExcludedMetricCount, mockExcludedMetrics);
    uint32_t metricCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateOperationGetReportFormatExp(mockMetricCalcOp.toHandle(), &metricCount, nullptr));
    EXPECT_EQ(metricCount, 1U);
    zet_metric_handle_t metricHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateOperationGetReportFormatExp(mockMetricCalcOp.toHandle(), &metricCount, &metricHandle));
    EXPECT_EQ(metricCount, 1U);
    EXPECT_EQ(metricHandle, mockMetricInReport.toHandle());
    EXPECT_EQ(metricCount, mockMetricCalcOp.getMetricsInReportCount());

    metricCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateOperationGetExcludedMetricsExp(mockMetricCalcOp.toHandle(), &metricCount, nullptr));
    EXPECT_EQ(metricCount, 1U);

    zet_metric_handle_t excludedMetricHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateOperationGetExcludedMetricsExp(mockMetricCalcOp.toHandle(), &metricCount, &excludedMetricHandle));
    EXPECT_EQ(metricCount, 1U);
    EXPECT_EQ(excludedMetricHandle, mockExcludedMetric.toHandle());
    EXPECT_NE(excludedMetricHandle, metricHandle);
}

using MetricRuntimeFixture = Test<DeviceFixture>;

TEST_F(MetricRuntimeFixture, WhenRunTimeEnableIsDoneThenReturnSuccess) {

    auto mockDeviceContext = new MockMetricDeviceContext(*device);
    mockDeviceContext->clearAllSources();
    auto metricSource = new MockMetricSource();
    metricSource->isAvailableReturn = true;
    mockDeviceContext->setMockMetricSource(metricSource);

    auto deviceImp = static_cast<DeviceImp *>(device);
    deviceImp->metricContext.reset(mockDeviceContext);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceEnableMetricsExp(device->toHandle()));
    deviceImp->metricContext.reset();
}

TEST_F(MetricRuntimeFixture, WhenRunTimeEnableIsDoneMultipleTimesThenEnableIsDoneOnlyOnce) {

    auto mockDeviceContext = new MockMetricDeviceContext(*device);
    mockDeviceContext->clearAllSources();
    auto metricSource = new MockMetricSource();
    metricSource->isAvailableReturn = true;
    mockDeviceContext->setMockMetricSource(metricSource);

    auto deviceImp = static_cast<DeviceImp *>(device);
    deviceImp->metricContext.reset(mockDeviceContext);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceEnableMetricsExp(device->toHandle()));
    EXPECT_EQ(metricSource->enableCallCount, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDeviceEnableMetricsExp(device->toHandle()));
    EXPECT_EQ(metricSource->enableCallCount, 1u);
    deviceImp->metricContext.reset();
}

TEST_F(MetricRuntimeFixture, WhenRunTimeEnableIsDoneAndNoSourcesAreAvailableThenReturnError) {

    auto mockDeviceContext = new MockMetricDeviceContext(*device);
    mockDeviceContext->clearAllSources();
    auto metricSource = new MockMetricSource();
    metricSource->isAvailableReturn = false;
    mockDeviceContext->setMockMetricSource(metricSource);

    auto deviceImp = static_cast<DeviceImp *>(device);
    deviceImp->metricContext.reset(mockDeviceContext);

    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zetDeviceEnableMetricsExp(device->toHandle()));
    EXPECT_EQ(metricSource->enableCallCount, 1u);
    deviceImp->metricContext.reset();
}

using MetricScopesFixture = Test<DeviceFixture>;

TEST_F(MetricScopesFixture, WhenGettingMetricScopeForSingleDeviceTheyAreCorrectlyEnumerated) {

    auto mockDeviceContext = new MockMetricDeviceContext(*device);
    mockDeviceContext->clearAllSources();
    auto metricSource = new MockMetricSource();
    metricSource->isAvailableReturn = true;
    mockDeviceContext->setMockMetricSource(metricSource);
    auto deviceImp = static_cast<DeviceImp *>(device);
    deviceImp->metricContext.reset(mockDeviceContext);

    uint32_t metricScopesCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            device->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));

    EXPECT_EQ(metricScopesCount, 1u);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopes(metricScopesCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            device->toHandle(),
                                                            &metricScopesCount,
                                                            metricScopes.data()));
    EXPECT_EQ(metricScopesCount, 1u);
    EXPECT_EQ(metricScopes.size(), 1u);
}

TEST_F(MetricScopesFixture, WhenMultipleSourcesAreAvailableComputeMetricScopesAreEnumeratedOnlyOnce) {

    auto mockDeviceContext = new MockMetricDeviceContext(*device);
    mockDeviceContext->clearAllSources();
    auto metricSource = new MockMetricSource();
    metricSource->isAvailableReturn = true;
    mockDeviceContext->setMockMetricSource(metricSource);

    auto metricSource2 = new MockMetricSource();
    metricSource2->isAvailableReturn = true;
    mockDeviceContext->setMockMetricSourceAtIndex(100, metricSource2);

    auto deviceImp = static_cast<DeviceImp *>(device);
    deviceImp->metricContext.reset(mockDeviceContext);

    uint32_t metricScopesCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            device->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));
    EXPECT_EQ(metricScopesCount, 1u);
}

TEST_F(MetricScopesFixture, GettingMetricsScopesPropertiesReturnsExpectedValues) {

    auto mockDeviceContext = new MockMetricDeviceContext(*device);
    mockDeviceContext->clearAllSources();
    auto metricSource = new MockMetricSource();
    metricSource->isAvailableReturn = true;
    mockDeviceContext->setMockMetricSource(metricSource);
    auto deviceImp = static_cast<DeviceImp *>(device);
    deviceImp->metricContext.reset(mockDeviceContext);

    uint32_t metricScopesCount = 1;
    zet_intel_metric_scope_exp_handle_t metricScope;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            device->toHandle(),
                                                            &metricScopesCount,
                                                            &metricScope));
    EXPECT_EQ(metricScopesCount, 1u);
    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopeGetPropertiesExp(metricScope,
                                                                     &scopeProperties));

    EXPECT_EQ(scopeProperties.iD, 0u);
    EXPECT_STREQ(scopeProperties.name, "COMPUTE_TILE_0");
    EXPECT_STREQ(scopeProperties.description, "Metrics results for tile 0");
}

TEST_F(MetricScopesFixture, MetricScopeObjectToAndFromHandleBaseClassWorkAsExpected) {

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    MockMetricScope mockMetricScope(scopeProperties);
    auto hMockScope = mockMetricScope.toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricScopeGetPropertiesExp(hMockScope,
                                                                                       &scopeProperties));
    auto mockScope = MetricScope::fromHandle(hMockScope);
    EXPECT_NE(nullptr, mockScope);
}

} // namespace ult
} // namespace L0
