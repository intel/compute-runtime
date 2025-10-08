/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

} // namespace ult
} // namespace L0
