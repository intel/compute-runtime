/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_ip_sampling_raw_data.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include <level_zero/zet_api.h>

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
class MockMetricIpSamplingOsInterface;

using EustallSupportedPlatforms = IsProduct<IGFX_PVC>;

constexpr uint32_t platformIpMetricCountXe = 10;
class MetricIpSamplingMultiDevFixture : public MultiDeviceFixture,
                                        public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    std::vector<MockMetricIpSamplingOsInterface *> osInterfaceVector = {};
    std::vector<L0::Device *> testDevices = {};
};

class MetricIpSamplingFixture : public DeviceFixture,
                                public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    DebugManagerStateRestore restorer;
    std::vector<MockMetricIpSamplingOsInterface *> osInterfaceVector = {};
};

class MetricIpSamplingCalculateBaseFixture {
  public:
    std::vector<MockRawDataHelper::RawReportElements> rawDataElements = {
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},                   // 1st raw report
        {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x000},        // 2nd raw report
        {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},                    // 3rd raw report
        {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},           // 4th raw report
        {100, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3}, // 5th raw report
        {100, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};        // 6th raw report

    // Raw reports are 64Bytes, 8 x uint64_t
    std::vector<std::array<uint64_t, 8>> rawReports = std::vector<std::array<uint64_t, 8>>(rawDataElements.size());
    size_t rawReportsBytesSize = 0;

    std::vector<std::string> expectedMetricNamesInReport = {"IP", "Active", "ControlStall", "PipeStall",
                                                            "SendStall", "DistStall", "SbidStall", "SyncStall",
                                                            "InstrFetchStall", "OtherStall"};

    std::vector<zet_typed_value_t> expectedMetricValues = {
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {11}}, // 1st raw report + 3rd raw report
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {10}},
        {ZET_VALUE_TYPE_UINT64, {110}}, // 2nd raw report + 4th raw report
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {100}},
        {ZET_VALUE_TYPE_UINT64, {210}}, // 5th raw report + 6th raw report
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}}};

    // Expected values when calculating only IP1 on first and third *raw* reports in rawReports
    std::vector<zet_typed_value_t> expectedMetricValuesIP1 = {
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
    };

    // Expected values when calculating only IP1 only on first *raw* reports in rawReports
    std::vector<zet_typed_value_t> expectedMetricValuesIP1FirstRawReport = {
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {2}},
        {ZET_VALUE_TYPE_UINT64, {4}},
        {ZET_VALUE_TYPE_UINT64, {5}},
        {ZET_VALUE_TYPE_UINT64, {6}},
        {ZET_VALUE_TYPE_UINT64, {7}},
        {ZET_VALUE_TYPE_UINT64, {8}},
        {ZET_VALUE_TYPE_UINT64, {9}},
        {ZET_VALUE_TYPE_UINT64, {10}},
        {ZET_VALUE_TYPE_UINT64, {3}}, // L0GfxCoreHelperHw arranges "OtherStall" metric at the end
    };

    // Expected values when calculating only IP1 only on third *raw* reports in rawReports
    std::vector<zet_typed_value_t> expectedMetricValuesIP1thirdRawReport = {
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {9}},
        {ZET_VALUE_TYPE_UINT64, {7}},
        {ZET_VALUE_TYPE_UINT64, {6}},
        {ZET_VALUE_TYPE_UINT64, {5}},
        {ZET_VALUE_TYPE_UINT64, {4}},
        {ZET_VALUE_TYPE_UINT64, {3}},
        {ZET_VALUE_TYPE_UINT64, {2}},
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {8}}, // L0GfxCoreHelperHw arranges "OtherStall" metric at the end
    };

    // Expected values when calculating only IP10, which is in second and fourth *raw* reports in rawReports
    std::vector<zet_typed_value_t> expectedMetricValuesIP10 = {
        {ZET_VALUE_TYPE_UINT64, {10}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
    };

    // Expected values when calculating only IP100, which is in fifth and sixth *raw* reports in rawReports
    std::vector<zet_typed_value_t> expectedMetricValuesIP100 = {
        {ZET_VALUE_TYPE_UINT64, {100}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}}};

    std::vector<MockRawDataHelper::RawReportElements> rawDataElementsOverflow = {
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},
        {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},
        {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x100}, // set the overflow bit in flags
        {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3}};
    // Raw reports are 64Bytes, 8 x uint64_t
    std::vector<std::array<uint64_t, 8>> rawReportsOverflow = std::vector<std::array<uint64_t, 8>>(rawDataElementsOverflow.size());
    size_t rawReportsBytesSizeOverflow = 0;

    std::vector<zet_typed_value_t> expectedMetricOverflowValues = {
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {10}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}}};

    MockMetricScope *mockMetricScope;
    zet_intel_metric_scope_exp_handle_t hMockScope = nullptr;
    zet_intel_metric_calculation_exp_desc_t calculationDesc{};
    zet_metric_group_handle_t metricGroupHandle = nullptr;

    void initRawReports();
    void initCalcDescriptor();
    void cleanupCalcDescriptor();
};

struct MetricIpSamplingCalculateSingleDevFixture : public MetricIpSamplingCalculateBaseFixture, MetricIpSamplingFixture {
  public:
    void SetUp() override;
    void TearDown() override;
};
struct MetricIpSamplingCalculateMultiDevFixture : public MetricIpSamplingCalculateBaseFixture, MetricIpSamplingMultiDevFixture {
  public:
    void SetUp() override;
    void TearDown() override;
};

struct MetricIpSamplingMetricsAggregationMultiDevFixture : public MetricIpSamplingCalculateBaseFixture, MetricIpSamplingMultiDevFixture {
  public:
    void SetUp() override;
    void TearDown() override;

    void initMultiRawReports();

    L0::Device *rootDevice = nullptr;
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

    std::vector<MockRawDataHelper::RawReportElements> rawDataElements2 = {
        {2, 10, 11, 12, 13, 14, 15, 16, 17, 18, 1000, 0x01},  // 1st raw report: new IP and values
        {100, 90, 91, 92, 93, 94, 95, 96, 97, 98, 1000, 0x3}, // 2nd raw report: repeated IP and new values from rawDataElements
        {2, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1000, 0x02},          // 3rd raw report
        {100, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x3}};         // 4th raw report

    // Raw reports are 64Bytes, 8 x uint64_t
    std::vector<std::array<uint64_t, 8>> rawReports2 = std::vector<std::array<uint64_t, 8>>(rawDataElements2.size());
    size_t rawReports2BytesSize = 0;

    std::vector<zet_typed_value_t> expectedMetricValues2 = {
        {ZET_VALUE_TYPE_UINT64, {2}},
        {ZET_VALUE_TYPE_UINT64, {20}}, // 1st raw report + 3rd raw report
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {100}},
        {ZET_VALUE_TYPE_UINT64, {99}}, // 2nd raw report + 4th raw report
        {ZET_VALUE_TYPE_UINT64, {99}},
        {ZET_VALUE_TYPE_UINT64, {99}},
        {ZET_VALUE_TYPE_UINT64, {99}},
        {ZET_VALUE_TYPE_UINT64, {99}},
        {ZET_VALUE_TYPE_UINT64, {99}},
        {ZET_VALUE_TYPE_UINT64, {99}},
        {ZET_VALUE_TYPE_UINT64, {99}},
        {ZET_VALUE_TYPE_UINT64, {99}}};

    // result types will always be ZET_VALUE_TYPE_UINT64 no need to check them always
    // Report format is: 10 Metrics ComputeScope 0, 10 Metrics ComputeScope 1
    std::vector<uint64_t> expectedMetricValuesCalcOpAllCompScopes = {
        // ---  first result report  ---
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, // Sub-dev 0 based on expectedMetricValues
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, // sub-dev 1 based on expectedMetricValues2
        // ---  second result report  ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, // Sub-dev 0
        100, 99, 99, 99, 99, 99, 99, 99, 99, 99,         // sub-dev 1
        // ---  third result report  ---
        100, 210, 210, 210, 210, 210, 210, 210, 210, 210, // Sub-dev 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0};                    // sub-dev 1, invalid

    // Report format is: 10 Metrics AggregatedScope
    std::vector<uint64_t> expectedMetricValuesCalcOpAggScope = {
        // ---  first result report  ---
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, // IP1 from sub-dev 0
        // ---  second result report  ---
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, // IP2 from sub-dev 1
        // ---  third result report ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, // IP10 from sub-dev 0
        // ---  fourth result report ---
        100, 309, 309, 309, 309, 309, 309, 309, 309, 309}; // IP100 from sub-dev 0 and sub-dev 1, summed values

    // Report format is: 10 Metrics AggregatedScope, 10 Metrics ComputeScope 0, 10 Metrics ComputeScope 1
    std::vector<uint64_t> expectedMetricValuesCalcOpAllScopes = {
        // ---  first result report  ---
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, // IP1 aggregated. Only available in sub-dev 0
        1, 11, 11, 11, 11, 11, 11, 11, 11, 11, // Sub-dev 0 based on expectedMetricValues
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20, // sub-dev 1 based on expectedMetricValues2
        // ---  second result report  ---
        2, 20, 20, 20, 20, 20, 20, 20, 20, 20,           // IP2 aggregated. Only available in sub-dev 1
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, // Sub-dev 0
        100, 99, 99, 99, 99, 99, 99, 99, 99, 99,         // sub-dev1
        // --- third result report ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110,  // IP10 from aggregated. Only available in sub-dev 0
        100, 210, 210, 210, 210, 210, 210, 210, 210, 210, // Sub-dev 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                     // sub-dev 1, invalid (no more IPs)
        // --- fourth result report ---
        100, 309, 309, 309, 309, 309, 309, 309, 309, 309, // IP100 aggregated, present in both sub-devs.
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                     // Sub-dev 0, invalid (no more IPs)
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0};                    // Sub-dev 1, invalid (no more IPs)

    std::vector<MockRawDataHelper::RawReportElements> rawDataElementsAppend = {
        {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1000, 0x01},         // 1st raw report: new IP and values
        {100, 90, 91, 92, 93, 94, 95, 96, 97, 98, 1000, 0x3}}; // 2nd raw report: repeated IP and new values from rawDataElements

    // Raw reports are 64Bytes, 8 x uint64_t
    std::vector<std::array<uint64_t, 8>> rawReportsAppend = std::vector<std::array<uint64_t, 8>>(rawDataElementsAppend.size());
    size_t rawReportsAppendBytesSize = 0;

    // Expected results for IP3 from rawDataElementsAppend
    std::vector<uint64_t> expectedMetricValuesIP3 = {
        3, 4, 6, 7, 8, 9, 10, 11, 12, 5};

    // Expected results for IP10 and IP100 after appending rawReportsAppend to original rawReports.
    // IP100 has entries in both so values are added
    std::vector<uint64_t> expectedMetricValuesIP10IP100AppendToRawReports = {
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110,
        100, 300, 302, 303, 304, 305, 306, 307, 308, 301};

    // Expected results for IP100 after appending rawReportsAppend to original rawReports2.
    // IP100 has entries in both so values are added
    std::vector<uint64_t> expectedMetricValuesIP100AppendToRawReports2 = {
        100, 189, 191, 192, 193, 194, 195, 196, 197, 190};

    std::vector<uint64_t> expectedMetricValuesAppendToAllComputeScopes = {
        /// ---  first result report  ---
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110,  // Sub-dev 0, IP10 from original rawReports
        100, 189, 191, 192, 193, 194, 195, 196, 197, 190, // Sub-dev 1, IP100 from original rawReports2 + rawReportsAppend
        // --- second result report  ---
        100, 300, 302, 303, 304, 305, 306, 307, 308, 301, // Sub-dev 0, IP100 from original rawReports + rawReportsAppend
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0                      // Sub-dev 1, invalid (no more IPs)
    };

    std::vector<uint64_t> expectedMetricValuesAppendToAggregatedScope = {
        3, 8, 12, 14, 16, 18, 20, 22, 24, 10,            // IP3 from rawReportsAppend *2 since was appended in both sub-devs
        10, 110, 110, 110, 110, 110, 110, 110, 110, 110, // IP10 from original rawReports
        100, 489, 493, 495, 497, 499, 501, 503, 505, 491 // IP100 from  rawReports + rawReports2 + appended * 2 since was appended in both sub-devs
    };

    // In aggregated scope all results will be valid. Expect three IPs: IP3, IP10, IP100
    // Sub-dev 0 has two IPs: IP10, IP100. So third report results will be invalid.
    // Sub-dev 1 has one IP: IP100. So, second and third report results will be invalid.
    std::vector<zet_intel_metric_result_exp_t> expectedMetricResultsAfterAppendForAllScopes = {
        // --- first result report  ---
        // * Aggregated Scope, IP 3 aggregated from both sub-devices
        {{3}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{8}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{12}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{14}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{16}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{18}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{20}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{22}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{24}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{10}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
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
        // * Compute Scope 2, IP 100 from rawReports2 + rawReportsAppend
        {{100}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{189}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{191}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{192}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{193}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{194}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{195}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{196}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{197}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{190}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
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
        // * Compute Scope 1, IP100 from rawReports + rawReportsAppend
        {{100}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{300}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{302}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{303}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{304}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{305}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{306}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{307}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{308}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{301}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
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
        // --- third result report  ---
        // * Aggregated scope, IP100 from rawReports + rawReports2 + appended from both sub-devs
        {{100}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{489}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{493}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{495}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{497}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{499}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{501}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{503}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{505}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
        {{491}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID},
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
        {{0}, ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_INVALID}};
};

} // namespace ult
} // namespace L0
