/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include "level_zero/zet_api.h"
#include "level_zero/zet_intel_gpu_metric.h"

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
    zet_metric_group_handle_t hMetricGroup = nullptr;

    MockMetricScope *mockMetricScope;
    zet_intel_metric_scope_exp_handle_t hMetricScope = nullptr;

    DebugManagerStateRestore restorer;
};

void CalcOperationFixture::SetUp() {
    DeviceFixture::setUp();
    mockMetricGroup = new MockMetricGroup(mockMetricSource);
    hMetricGroup = mockMetricGroup->toHandle();

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    mockMetricScope = new MockMetricScope(scopeProperties, false);
    hMetricScope = mockMetricScope->toHandle();
}

void CalcOperationFixture::TearDown() {
    DeviceFixture::tearDown();
    delete mockMetricGroup;
    delete mockMetricScope;
    hMetricGroup = nullptr;
    hMetricScope = nullptr;
}

class MetricScopesMultiDeviceFixture : public MultiDeviceFixture,
                                       public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;

    L0::Device *rootDevice = nullptr;
    L0::Device *subDevice = nullptr;
    MockMetricDeviceContext *mockRootDeviceContext = nullptr;
    MockMetricSource *mockRootMetricSource = nullptr;

    MockMetricDeviceContext *mockSubDeviceContext = nullptr;
    MockMetricSource *mockSubMetricSource = nullptr;

    DebugManagerStateRestore restorer;
};

void MetricScopesMultiDeviceFixture::SetUp() {

    debugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixture::numRootDevices = 1;
    MultiDeviceFixture::numSubDevices = 2;

    MultiDeviceFixture::setUp();

    rootDevice = driverHandle->devices[0];
    auto &rootDeviceImp = *static_cast<DeviceImp *>(rootDevice);
    subDevice = rootDeviceImp.subDevices[0];
    auto &subDeviceImp = *static_cast<DeviceImp *>(subDevice);

    mockRootMetricSource = new MockMetricSource();
    mockRootMetricSource->isAvailableReturn = true;

    mockRootDeviceContext = new MockMetricDeviceContext(*rootDevice);
    mockRootDeviceContext->clearAllSources();
    mockRootDeviceContext->setMockMetricSource(mockRootMetricSource);
    mockRootDeviceContext->setMultiDeviceCapable(true);
    rootDeviceImp.metricContext.reset(mockRootDeviceContext);

    mockSubMetricSource = new MockMetricSource();
    mockSubMetricSource->isAvailableReturn = true;

    mockSubDeviceContext = new MockMetricDeviceContext(*subDevice);
    mockSubDeviceContext->clearAllSources();
    mockSubDeviceContext->setMockMetricSource(mockSubMetricSource);
    mockSubDeviceContext->setMultiDeviceCapable(false);
    subDeviceImp.metricContext.reset(mockSubDeviceContext);
}

void MetricScopesMultiDeviceFixture::TearDown() {
    MultiDeviceFixture::tearDown();

    mockRootMetricSource = nullptr;
    mockRootDeviceContext = nullptr;
    rootDevice = nullptr;
    subDevice = nullptr;
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpAndInvalidParamsPassedThenErrorIsHandled) {

    // Aggregation window is zero
    zet_intel_metric_calculation_exp_desc_t calculationDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP,
        nullptr,       // pNext
        1,             // metricGroupCount
        &hMetricGroup, // phMetricGroups
        0,             // metricCount
        nullptr,       // phMetrics
        0,             // timeWindowsCount
        nullptr,       // pCalculationTimeWindows
        0,             // timeAggregationWindow
        1,             // metricScopesCount
        &hMetricScope, // phMetricScopes
    };

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    // No metric groups or metrics
    calculationDesc.timeAggregationWindow = 100;
    calculationDesc.metricGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                            device->toHandle(), &calculationDesc,
                                                                                            &hCalculationOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpWithMixedSourcesThenErrorIsReturned) {

    MockMetricSource mockMetricSource2{};
    mockMetricSource2.setType(UINT32_MAX);
    MockMetricGroup mockMetricGroup2(mockMetricSource2);

    // metric groups from different source
    std::vector<zet_metric_group_handle_t> metricGroups{hMetricGroup, mockMetricGroup2.toHandle()};
    zet_intel_metric_calculation_exp_desc_t calculationDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP,
        nullptr,             // pNext
        2,                   // metricGroupCount
        metricGroups.data(), // phMetricGroups
        0,                   // metricCount
        nullptr,             // phMetrics
        0,                   // timeWindowsCount
        nullptr,             // pCalculationTimeWindows
        1000,                // timeAggregationWindow
        1,                   // metricScopesCount
        &hMetricScope,       // phMetricScopes
    };

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculationOperationCreateExp(context,
                                                                                            device->toHandle(), &calculationDesc,
                                                                                            &hCalculationOperation));

    // metrics from different source
    MockMetric mockMetric(mockMetricSource);
    MockMetric mockMetric2(mockMetricSource2);
    std::vector<zet_metric_handle_t> metrics{mockMetric.toHandle(), mockMetric2.toHandle()};

    calculationDesc.metricGroupCount = 1;
    calculationDesc.phMetricGroups = &hMetricGroup;
    calculationDesc.metricCount = 2;
    calculationDesc.phMetrics = metrics.data();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculationOperationCreateExp(context,
                                                                                            device->toHandle(), &calculationDesc,
                                                                                            &hCalculationOperation));

    // metrics and metric group from different source
    calculationDesc.metricCount = 1;
    calculationDesc.phMetrics = &metrics[1];
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculationOperationCreateExp(context,
                                                                                            device->toHandle(), &calculationDesc,
                                                                                            &hCalculationOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpWithMixedHierarchiesThenErrorIsReturned) {
    MockMetricGroup mockMetricGroup2(mockMetricSource);
    mockMetricGroup2.isMultiDevice = true;

    // metric groups from different hierarchy
    std::vector<zet_metric_group_handle_t> metricGroups{hMetricGroup, mockMetricGroup2.toHandle()};
    zet_intel_metric_calculation_exp_desc_t calculationDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP,
        nullptr,             // pNext
        2,                   // metricGroupCount
        metricGroups.data(), // phMetricGroups
        0,                   // metricCount
        nullptr,             // phMetrics
        0,                   // timeWindowsCount
        nullptr,             // pCalculationTimeWindows
        1000,                // timeAggregationWindow
        1,                   // metricScopesCount
        &hMetricScope,       // phMetricScopes
    };

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculationOperationCreateExp(context,
                                                                                            device->toHandle(), &calculationDesc,
                                                                                            &hCalculationOperation));

    MockMetric mockMetric(mockMetricSource), mockMetric2(mockMetricSource);
    mockMetric2.setMultiDevice(true);

    std::vector<zet_metric_handle_t> metrics{mockMetric.toHandle(), mockMetric2.toHandle()};

    calculationDesc.metricGroupCount = 1;
    calculationDesc.phMetricGroups = &hMetricGroup;
    calculationDesc.metricCount = 2;
    calculationDesc.phMetrics = metrics.data();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculationOperationCreateExp(context,
                                                                                            device->toHandle(), &calculationDesc,
                                                                                            &hCalculationOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpUseTheSourceFromMetricGroupOrMetricWhenAvailable) {

    MockMetricGroup mockMetricGroup2(mockMetricSource);

    std::vector<zet_metric_group_handle_t> metricGroups{hMetricGroup, mockMetricGroup2.toHandle()};
    zet_intel_metric_calculation_exp_desc_t calculationDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP,
        nullptr,             // pNext
        2,                   // metricGroupCount
        metricGroups.data(), // phMetricGroups
        0,                   // metricCount
        nullptr,             // phMetrics
        0,                   // timeWindowsCount
        nullptr,             // pCalculationTimeWindows
        1000,                // timeAggregationWindow
        1,                   // metricScopesCount
        &hMetricScope,       // phMetricScopes
    };

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context,
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    EXPECT_NE(nullptr, hCalculationOperation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

    MockMetric mockMetric(mockMetricSource), mockMetric2(mockMetricSource);
    std::vector<zet_metric_handle_t> metrics{mockMetric.toHandle(), mockMetric.toHandle()};

    calculationDesc.metricGroupCount = 0;
    calculationDesc.phMetricGroups = nullptr;
    calculationDesc.metricCount = 2;
    calculationDesc.phMetrics = metrics.data();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context,
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    EXPECT_NE(nullptr, hCalculationOperation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

TEST_F(CalcOperationFixture, WhenCreatingCalcOpObjectToAndFromHandleBaseClassWorkAsExpected) {
    std::vector<MetricImp *> mockMetricsInReport{};
    std::vector<MetricImp *> mockExcludedMetrics{};

    std::vector<MetricScopeImp *> metricScopes{mockMetricScope};

    MockMetricCalcOp mockMetricCalcOp(false, metricScopes, mockMetricsInReport, mockExcludedMetrics);
    auto hMockCalcOp = mockMetricCalcOp.toHandle();

    EXPECT_EQ(false, mockMetricCalcOp.isRootDevice());
    auto mockCalcOp = MetricCalcOp::fromHandle(hMockCalcOp);
    EXPECT_NE(nullptr, mockCalcOp);
}

TEST_F(CalcOperationFixture, WhenGettingMetricsInReportAndExcludedMetricsThenTheyAreReturnedCorrectly) {
    std::vector<MetricImp *> mockMetricsInReport{};
    MockMetric mockMetricInReport(mockMetricSource);
    mockMetricsInReport.push_back(&mockMetricInReport);

    std::vector<MetricImp *> mockExcludedMetrics{};
    MockMetric mockExcludedMetric(mockMetricSource);
    mockExcludedMetrics.push_back(&mockExcludedMetric);

    std::vector<MetricScopeImp *> metricScopes{mockMetricScope};

    MockMetricCalcOp mockMetricCalcOp(false, metricScopes, mockMetricsInReport, mockExcludedMetrics);
    uint32_t metricCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(mockMetricCalcOp.toHandle(), &metricCount, nullptr, nullptr));
    EXPECT_EQ(metricCount, 1U);
    zet_metric_handle_t metricHandle = nullptr;
    zet_intel_metric_scope_exp_handle_t metricScopeHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(mockMetricCalcOp.toHandle(), &metricCount, &metricHandle, &metricScopeHandle));
    EXPECT_EQ(metricCount, 1U);
    EXPECT_EQ(metricHandle, mockMetricInReport.toHandle());
    EXPECT_EQ(metricScopeHandle, mockMetricScope->toHandle());
    EXPECT_EQ(metricCount, mockMetricCalcOp.getMetricsInReportCount());

    metricCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetExcludedMetricsExp(mockMetricCalcOp.toHandle(), &metricCount, nullptr));
    EXPECT_EQ(metricCount, 1U);

    zet_metric_handle_t excludedMetricHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetExcludedMetricsExp(mockMetricCalcOp.toHandle(), &metricCount, &excludedMetricHandle));
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

TEST_F(MetricScopesMultiDeviceFixture, WhenGettingMetricScopesForSubDeviceTheyAreCorrectlyEnumerated) {

    uint32_t metricScopesCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            subDevice->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));

    EXPECT_EQ(metricScopesCount, 1u);
    zet_intel_metric_scope_exp_handle_t metricScope;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            subDevice->toHandle(),
                                                            &metricScopesCount,
                                                            &metricScope));
    EXPECT_EQ(metricScopesCount, 1u);

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopeGetPropertiesExp(metricScope,
                                                                     &scopeProperties));

    // ID of compute scopes should be 0, sub-devices support single compute scope
    std::string expectedName = computeScopeNamePrefix.data() + std::to_string(0);
    EXPECT_STREQ(scopeProperties.name, expectedName.c_str());
    std::string expectedDescription = computeScopeDescriptionPrefix.data() + std::to_string(0);
    EXPECT_STREQ(scopeProperties.description, expectedDescription.c_str());
}

TEST_F(MetricScopesMultiDeviceFixture, WhenMultipleSourcesAreAvailableComputeMetricScopesAreEnumeratedOnlyOnce) {

    auto metricSource2 = new MockMetricSource();
    metricSource2->isAvailableReturn = true;
    mockSubDeviceContext->setMockMetricSourceAtIndex(100, metricSource2);

    uint32_t metricScopesCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            subDevice->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));
    EXPECT_EQ(metricScopesCount, 1u);
}

TEST_F(MetricScopesMultiDeviceFixture, MetricScopeObjectToAndFromHandleBaseClassWorkAsExpected) {

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    MockMetricScope mockMetricScope(scopeProperties, false);
    auto hMockScope = mockMetricScope.toHandle();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricScopeGetPropertiesExp(hMockScope,
                                                                                       &scopeProperties));
    auto mockScope = MetricScope::fromHandle(hMockScope);
    EXPECT_NE(nullptr, mockScope);
}

template <typename GfxFamily>
struct MockL0GfxCoreHelperNOTSupportMetricsAggregation : L0::L0GfxCoreHelperHw<GfxFamily> {
    bool supportMetricsAggregation() const override { return false; }
};

HWTEST_F(MetricScopesMultiDeviceFixture, GivenRootDeviceThatDoesNOTSupportsMetricsAggregationWhenGettingMetricScopesComputeScopesIDMatchSubdevice) {

    MockL0GfxCoreHelperNOTSupportMetricsAggregation<FamilyType> mockL0GfxCoreHelper;
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelper));
    rootDevice->getNEODevice()->getRootDeviceEnvironmentRef().apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    uint32_t metricScopesCount = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            rootDevice->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));
    EXPECT_EQ(metricScopesCount, numSubDevices);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopes(metricScopesCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            rootDevice->toHandle(),
                                                            &metricScopesCount,
                                                            metricScopes.data()));
    EXPECT_EQ(metricScopesCount, numSubDevices);
    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    for (uint32_t i = 0; i < metricScopesCount; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopeGetPropertiesExp(metricScopes[i],
                                                                         &scopeProperties));
        EXPECT_EQ(scopeProperties.iD, i);
        // ID of compute scopes should be sub-dev ID
        std::string expectedName = computeScopeNamePrefix.data() + std::to_string(i);
        EXPECT_STREQ(scopeProperties.name, expectedName.c_str());
        std::string expectedDescription = computeScopeDescriptionPrefix.data() + std::to_string(i);
        EXPECT_STREQ(scopeProperties.description, expectedDescription.c_str());
    }

    rootDevice->getNEODevice()->getRootDeviceEnvironmentRef().apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    l0GfxCoreHelperBackup.release();
}

template <typename GfxFamily>
struct MockL0GfxCoreHelperSupportMetricsAggregation : L0::L0GfxCoreHelperHw<GfxFamily> {
    bool supportMetricsAggregation() const override { return true; }
};

HWTEST_F(MetricScopesMultiDeviceFixture, GivenRootDeviceThatSupportsMetricsAggregationWhenGettingMetricScopesAggregatedScopeShouldBeFirst) {

    MockL0GfxCoreHelperSupportMetricsAggregation<FamilyType> mockL0GfxCoreHelper;
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelper));
    rootDevice->getNEODevice()->getRootDeviceEnvironmentRef().apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);

    uint32_t metricScopesCount = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            rootDevice->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));
    EXPECT_EQ(metricScopesCount, numRootDevices + numSubDevices);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopes(metricScopesCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            rootDevice->toHandle(),
                                                            &metricScopesCount,
                                                            metricScopes.data()));
    EXPECT_EQ(metricScopesCount, numRootDevices + numSubDevices);
    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopeGetPropertiesExp(metricScopes[0],
                                                                     &scopeProperties));

    // Aggregated scope must be first with ID 0
    EXPECT_EQ(scopeProperties.iD, 0u);
    EXPECT_STREQ(scopeProperties.name, aggregatedScopeName.data());
    EXPECT_STREQ(scopeProperties.description, aggregatedScopeDescription.data());

    for (uint32_t i = 1; i < metricScopesCount; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopeGetPropertiesExp(metricScopes[i],
                                                                         &scopeProperties));
        EXPECT_EQ(scopeProperties.iD, i);
        // ID of compute scopes should be sub-dev ID+1 (since aggregated scope is 0)
        std::string expectedName = computeScopeNamePrefix.data() + std::to_string(i - 1);
        EXPECT_STREQ(scopeProperties.name, expectedName.c_str());
        std::string expectedDescription = computeScopeDescriptionPrefix.data() + std::to_string(i - 1);
        EXPECT_STREQ(scopeProperties.description, expectedDescription.c_str());
    }

    rootDevice->getNEODevice()->getRootDeviceEnvironmentRef().apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    l0GfxCoreHelperBackup.release();
}

HWTEST_F(MetricScopesMultiDeviceFixture, GivenMetricsAggregationIsSupportedWhenCreatingCalcOpAggregatedScopeShouldBeFirstIntheOutputReport) {

    MockL0GfxCoreHelperSupportMetricsAggregation<FamilyType> mockL0GfxCoreHelper;
    std::unique_ptr<ApiGfxCoreHelper> l0GfxCoreHelperBackup(static_cast<ApiGfxCoreHelper *>(&mockL0GfxCoreHelper));
    rootDevice->getNEODevice()->getRootDeviceEnvironmentRef().apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);

    uint32_t metricScopesCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            rootDevice->toHandle(),
                                                            &metricScopesCount,
                                                            nullptr));
    EXPECT_EQ(metricScopesCount, numRootDevices + numSubDevices);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopes(metricScopesCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopesGetExp(context->toHandle(),
                                                            rootDevice->toHandle(),
                                                            &metricScopesCount,
                                                            metricScopes.data()));

    EXPECT_EQ(metricScopesCount, numRootDevices + numSubDevices);

    MockMetricGroup mockMetricGroup(*mockRootMetricSource);
    mockMetricGroup.isMultiDevice = true;
    zet_metric_group_handle_t hMetricGroup = mockMetricGroup.toHandle();

    // send aggregated scope last
    std::swap(metricScopes[0], metricScopes[metricScopesCount - 1]);

    // Aggregation window is zero
    zet_intel_metric_calculation_exp_desc_t calculationDesc{
        ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATION_DESC_EXP,
        nullptr,             // pNext
        1,                   // metricGroupCount
        &hMetricGroup,       // phMetricGroups
        0,                   // metricCount
        nullptr,             // phMetrics
        0,                   // timeWindowsCount
        nullptr,             // pCalculationTimeWindows
        100,                 // timeAggregationWindow
        metricScopesCount,   // metricScopesCount
        metricScopes.data(), // phMetricScopes
    };

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    EXPECT_NE(nullptr, hCalculationOperation);

    uint32_t metricCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricCount, nullptr, nullptr));

    // Expect one metric per scope requested: MockMetricSource::calcOperationCreate() will create one mock metric per metric group
    EXPECT_EQ(metricCount, metricScopesCount);
    std::vector<zet_metric_handle_t> metricHandles(metricCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopeHandles(metricCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricCount, metricHandles.data(), metricScopeHandles.data()));

    for (uint32_t i = 0; i < metricCount; i++) {
        zet_intel_metric_scope_properties_exp_t scopeProperties{};
        scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
        scopeProperties.pNext = nullptr;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopeGetPropertiesExp(metricScopeHandles[i],
                                                                         &scopeProperties));
        if (i == 0) {
            // Aggregated scope must be first with ID 0
            EXPECT_EQ(scopeProperties.iD, 0u);
            EXPECT_STREQ(scopeProperties.name, aggregatedScopeName.data());
            EXPECT_STREQ(scopeProperties.description, aggregatedScopeDescription.data());
        } else {
            EXPECT_EQ(scopeProperties.iD, i);
            // ID of compute scopes should be sub-dev ID+1 (since aggregated scope is 0)
            std::string expectedName = computeScopeNamePrefix.data() + std::to_string(i - 1);
            EXPECT_STREQ(scopeProperties.name, expectedName.c_str());
            std::string expectedDescription = computeScopeDescriptionPrefix.data() + std::to_string(i - 1);
            EXPECT_STREQ(scopeProperties.description, expectedDescription.c_str());
        }
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

    rootDevice->getNEODevice()->getRootDeviceEnvironmentRef().apiGfxCoreHelper.swap(l0GfxCoreHelperBackup);
    l0GfxCoreHelperBackup.release();
}

} // namespace ult
} // namespace L0
