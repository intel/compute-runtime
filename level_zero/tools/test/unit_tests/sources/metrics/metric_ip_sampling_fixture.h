/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_test_hw_helper.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include <level_zero/zet_api.h>

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
class MockMetricIpSamplingOsInterface;

class MockRawDataHelper {
  public:
    MockRawDataHelper() = default;
    ~MockRawDataHelper() = default;

    static void addMultiSubDevHeader(uint8_t *rawDataOut, size_t rawDataOutSize, uint8_t *rawDataIn, size_t rawDataInSize, uint32_t setIndex) {
        const auto expectedOutSize = sizeof(IpSamplingMultiDevDataHeader) + rawDataInSize;
        if (expectedOutSize <= rawDataOutSize) {

            auto header = reinterpret_cast<IpSamplingMultiDevDataHeader *>(rawDataOut);
            header->magic = IpSamplingMultiDevDataHeader::magicValue;
            header->rawDataSize = static_cast<uint32_t>(rawDataInSize);
            header->setIndex = setIndex;
            memcpy_s(rawDataOut + sizeof(IpSamplingMultiDevDataHeader), rawDataOutSize - sizeof(IpSamplingMultiDevDataHeader),
                     rawDataIn, rawDataInSize);
        }
    }
};

class MetricIpSamplingMultiDevFixture : public MultiDeviceFixture,
                                        public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    static zet_metric_group_handle_t getMetricGroupForDevice(L0::Device *device);
    std::vector<MockMetricIpSamplingOsInterface *> osInterfaceVector = {};
    std::vector<L0::Device *> testDevices = {};
    std::vector<L0::Device *> rootOneSubDev = {};
    IpSamplingTestProductHelper *ipSamplingTestProductHelper = {};
};

class MetricIpSamplingFixture : public DeviceFixture,
                                public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    DebugManagerStateRestore restorer;
    std::vector<MockMetricIpSamplingOsInterface *> osInterfaceVector = {};
    IpSamplingTestProductHelper *ipSamplingTestProductHelper = {};
};

class MetricIpSamplingCalculateBaseFixture {
  public:
    std::vector<std::array<uint64_t, 8>> rawReports = {};
    size_t rawReportsBytesSize = 0;

    void initRawReports(IpSamplingTestProductHelper *ipSamplingTestProductHelper, PRODUCT_FAMILY productFamily);
};

// Fixture used for zetMetricGroupCalculateMultipleMetricValuesExp() and zetMetricGroupCalculateMetricValues()
struct MetricIpSamplingCalculateMetricGroupFixture : public MetricIpSamplingCalculateBaseFixture, MetricIpSamplingMultiDevFixture {
  public:
    void SetUp() override;
    void TearDown() override;
    L0::Device *rootDevice = nullptr;
};
// Fixture used for APIs that use zet_intel_metric_calculation_operation_exp_handle_t
struct MetricIpSamplingCalculateOperationFixture : public MetricIpSamplingCalculateBaseFixture, MetricIpSamplingMultiDevFixture {
  public:
    void SetUp() override;
    void TearDown() override;
    void initCalcDescHandles(L0::ContextImp *context,
                             L0::Device *device);
    void cleanUpHandles();

    L0::Device *rootDevice = nullptr;
    L0::Device *subDevice = nullptr;
    std::map<L0::Device *, zet_metric_group_handle_t> metricGroupHandlePerDevice{};
    std::map<L0::Device *, std::vector<zet_intel_metric_scope_exp_handle_t>> scopesPerDevice{};
    std::map<L0::Device *, zet_intel_metric_calculation_exp_desc_t> calcDescPerDevice{};
};

// Fixture for corner cases of zetIntelMetricCalculateValuesExp()
struct MetricIpSamplingMetricsAggregationMultiDevFixture : public MetricIpSamplingCalculateBaseFixture, MetricIpSamplingMultiDevFixture {
  public:
    void SetUp() override;
    void TearDown() override;

    void initExtendedRawReports();

    L0::Device *rootDevice = nullptr;
    zet_metric_group_handle_t rootDevMetricGroupHandle = nullptr;

    MockMetricScope *mockMetricScopeCompute0 = nullptr;
    zet_intel_metric_scope_exp_handle_t hMockScopeCompute0 = nullptr;
    MockMetricScope *mockMetricScopeCompute1 = nullptr;
    zet_intel_metric_scope_exp_handle_t hMockScopeCompute1 = nullptr;
    MockMetricScope *mockMetricScopeAggregated = nullptr;
    zet_intel_metric_scope_exp_handle_t hMockScopeAggregated = nullptr;

    std::vector<zet_intel_metric_scope_exp_handle_t> hComputeMetricScopes;
    std::vector<zet_intel_metric_scope_exp_handle_t> hAllScopes;

    zet_intel_metric_calculation_operation_exp_handle_t hCalcOpCompScope1 = nullptr;
    zet_intel_metric_calculation_operation_exp_handle_t hCalcOpCompScope2 = nullptr;
    zet_intel_metric_calculation_operation_exp_handle_t hCalcOpAllCompScopes = nullptr;
    zet_intel_metric_calculation_operation_exp_handle_t hCalcOpAggScope = nullptr;
    zet_intel_metric_calculation_operation_exp_handle_t hCalcOpAllScopes = nullptr;
    std::vector<zet_intel_metric_calculation_operation_exp_handle_t> hCalcOps;

    // Raw reports are 64Bytes, 8 x uint64_t
    std::vector<std::array<uint64_t, 8>> rawReports2 = {};
    size_t rawReports2BytesSize = 0;

    std::vector<uint64_t> expectedMetricValues2Xe2AndLater = {
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,   // 1st raw report + 3rd raw report of secondRawDataElementsXe2AndLater
        100, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, // 2nd raw report + 4th raw report of secondRawDataElementsXe2AndLater
    };

    // Report format is: 11 Metrics ComputeScope 0, 11 Metrics ComputeScope 1
    // asuming rawreports for sub-dev0  and rawreports2 for sub-dev1
    std::vector<uint64_t> expectedMetricValuesCalcOpAllCompScopesXe2AndLater = {
        // ---  first result report  ---
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, // Sub-dev 0 based on expectedMetricValues
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, // sub-dev 1 based on expectedMetricValues2
        // ---  second result report ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, // Sub-dev 0
        100, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,          // sub-dev 1
        // ---  third result report  ---
        100, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, // Sub-dev 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};                      // sub-dev 1, invalid

    // Report format is: 11 Metrics AggregatedScope
    // asuming rawreports for sub-dev0  and rawreports2 for sub-dev1,
    std::vector<uint64_t> expectedMetricValuesCalcOpAggScopeXe2AndLater = {
        // ---  first result report  ---
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, // IP1 from sub-dev 0
        // ---  second result report  ---
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, // IP2 from sub-dev 1
        // ---  third result report ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, // IP10 from sub-dev 0
        // ---  fourth result report ---
        100, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309}; // IP100 from sub-dev 0 and sub-dev 1, summed values

    // Report format is: 10 Metrics AggregatedScope, 10 Metrics ComputeScope 0, 10 Metrics ComputeScope 1
    std::vector<uint64_t> expectedMetricValuesCalcOpAllScopesXe2AndLater = {
        // ---  first result report  ---
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, // IP1 aggregated. Only available in sub-dev 0
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, // Sub-dev 0 based on rawDataElements
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, // sub-dev 1 based on rawDataElements2
        // ---  second result report  ---
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,            // IP2 aggregated. Only available in sub-dev 1
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, // Sub-dev 0
        100, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,          // sub-dev1
        // --- third result report ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,  // IP10 from aggregated. Only available in sub-dev 0
        100, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, // Sub-dev 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // sub-dev 1, invalid (no more IPs)
        // --- fourth result report ---
        100, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, // IP100 aggregated, present in both sub-devs.
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       // Sub-dev 0, invalid (no more IPs)
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};                      // Sub-dev 1, invalid (no more IPs)

    // Raw reports are 64Bytes, 8 x uint64_t
    std::vector<std::array<uint64_t, 8>> rawReportsAppend = {};
    size_t rawReportsAppendBytesSize = 0;

    // Expected results for IP3 from correlating metricNameToRawElementIndexXe2AndLater and rawDataElementsAppendXe2AndLater
    std::vector<uint64_t> expectedMetricValuesIP3Xe2AndLater = {
        3, 5, 4, 7, 8, 9, 10, 11, 12, 13, 6};

    // Expected results for IP10 and IP100 after appending rawReportsAppend (rawDataElementsAppendXe2AndLater) to original rawReports.
    // IP100 has entries in both so values are added. Use metricNameToRawElementIndexXe2AndLater for correlation of raw elements and expected results
    std::vector<uint64_t> expectedMetricValuesIP10IP100AppendToRawReportsXe2AndLater = {
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
        100, 301, 300, 303, 304, 305, 306, 307, 308, 309, 302};

    // Expected results for IP100 after appending rawReportsAppend (rawDataElementsAppendXe2AndLater) to original rawReports2 (secondRawDataElementsXe2AndLater).
    // IP100 has entries in both so values are added
    std::vector<uint64_t> expectedMetricValuesIP100AppendToRawReportsXe2AndLater = {
        100, 190, 189, 192, 193, 194, 195, 196, 197, 198, 191};

    std::vector<uint64_t> expectedMetricValuesAppendToAllComputeScopesXe2AndLater = {
        /// ---  first result report  ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,  // Sub-dev 0, IP10 from original rawReports
        100, 190, 189, 192, 193, 194, 195, 196, 197, 198, 191, // Sub-dev 1, IP100 from original rawReports2 + rawReportsAppend
        // --- second result report  ---
        100, 301, 300, 303, 304, 305, 306, 307, 308, 309, 302, // Sub-dev 0, IP100 from original rawReports + rawReportsAppend
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                        // Sub-dev 1, invalid (no more IPs)
    };

    std::vector<uint64_t> expectedMetricValuesAppendToAggregatedScopeXe2AndLater = {
        3, 10, 8, 14, 16, 18, 20, 22, 24, 26, 12,             // IP3 from rawReportsAppend *2 since was appended in both sub-devs
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, // IP10 from original rawReports
        100, 491, 489, 495, 497, 499, 501, 503, 505, 507, 493 // IP100 from  rawReports + rawReports2 + appended * 2 since was appended in both sub-devs
    };

    // In aggregated scope all results will be valid. Expect three IPs: IP3, IP10, IP100
    // Sub-dev 0 has two IPs: IP10, IP100. So third report results will be invalid.
    // Sub-dev 1 has one IP: IP100. So, second and third report results will be invalid.
    std::vector<zet_intel_metric_result_exp_t> expectedMetricResultsAfterAppendForAllScopesXe2AndLater = {
        // --- first result report  ---
        // * Aggregated Scope, IP 3 aggregated from both sub-devices
        {{3}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{10}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{8}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{14}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{16}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{18}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{20}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{22}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{24}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{26}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{12}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        // * Compute Scope 1, IP 10 from rawReports
        {{10}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        // * Compute Scope 2, IP 100 from rawReports2 + rawReportsAppend
        {{100}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{190}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{189}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{192}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{193}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{194}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{195}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{196}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{197}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{198}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{191}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        // --- second result report  ---
        // * Aggregated scope, IP10 from rawReports
        {{10}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{110}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        // * Compute Scope 1, IP100 from rawReports + rawReportsAppend
        {{100}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{301}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{300}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{303}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{304}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{305}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{306}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{307}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{308}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{309}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{302}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        // * Compute Scope 2, invalid (no more IPs)
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        // --- third result report  ---
        // * Aggregated scope, IP100 from rawReports + rawReports2 + appended from both sub-devs
        {{100}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{491}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{489}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{495}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{497}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{499}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{501}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{503}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{505}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{507}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{493}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        // * Compute Scope 1, invalid (no more IPs)
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        // * Compute Scope 2, invalid (no more IPs)
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID},
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID}};
};

} // namespace ult
} // namespace L0
