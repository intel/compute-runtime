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
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include <level_zero/zet_api.h>

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
class MockMetricIpSamplingOsInterface;

using EustallSupportedPlatforms = IsProduct<IGFX_PVC>;

class MockStallRawIpData {
  public:
    static constexpr uint32_t ipShift = 29;
    static constexpr uint32_t ipMask = 0x1fffffff;

    static constexpr uint32_t byteShift = 8;
    static constexpr uint32_t byteMask = 0xff;

    static constexpr uint32_t wordShift = 16;
    static constexpr uint32_t wordMask = 0xffff;

    uint64_t rawData[8];
    MockStallRawIpData(uint64_t ip, uint64_t activeCount, uint64_t otherCount, uint64_t controlCount,
                       uint64_t pipeStallCount, uint64_t sendCount, uint64_t distAccCount,
                       uint64_t sbidCount, uint64_t syncCount, uint64_t instFetchCount, uint64_t subSlice,
                       uint64_t flags) {

        rawData[0] = (ip & ipMask) |
                     ((activeCount & byteMask) << ipShift) |
                     ((otherCount & byteMask) << (ipShift + byteShift)) |
                     ((controlCount & byteMask) << (ipShift + 2 * byteShift)) |
                     ((pipeStallCount & byteMask) << (ipShift + 3 * byteShift)) |
                     ((sendCount & 0x7) << (ipShift + 4 * byteShift));

        rawData[1] = ((sendCount & 0xf8) >> 3) |
                     ((distAccCount & byteMask) << 5) |
                     ((sbidCount & byteMask) << (5 + byteShift)) |
                     ((syncCount & byteMask) << (5 + 2 * byteShift)) |
                     ((instFetchCount & byteMask) << (5 + 3 * byteShift));

        rawData[2] = 0LL;
        rawData[3] = 0LL;
        rawData[4] = 0LL;
        rawData[5] = 0LL;
        rawData[6] = (subSlice & wordMask) | ((flags & wordMask) << wordShift);
        rawData[7] = 0;
    }
};

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
    void addHeader(uint8_t *rawDataOut, size_t rawDataOutSize, uint8_t *rawDataIn, size_t rawDataInSize, uint32_t setIndex) {

        const auto expectedOutSize = sizeof(IpSamplingMetricDataHeader) + rawDataInSize;
        if (expectedOutSize <= rawDataOutSize) {

            auto header = reinterpret_cast<IpSamplingMetricDataHeader *>(rawDataOut);
            header->magic = IpSamplingMetricDataHeader::magicValue;
            header->rawDataSize = static_cast<uint32_t>(rawDataInSize);
            header->setIndex = setIndex;
            memcpy_s(rawDataOut + sizeof(IpSamplingMetricDataHeader), rawDataOutSize - sizeof(IpSamplingMetricDataHeader),
                     rawDataIn, rawDataInSize);
        }
    }

    std::vector<MockStallRawIpData> rawDataVector = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},
                                                     {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x000},
                                                     {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},
                                                     {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},
                                                     {100, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3},
                                                     {100, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};

    std::vector<std::string> expectedMetricNamesInReport = {"IP", "Active", "ControlStall", "PipeStall",
                                                            "SendStall", "DistStall", "SbidStall", "SyncStall",
                                                            "InstrFetchStall", "OtherStall"};

    size_t rawDataVectorSize = sizeof(rawDataVector[0]) * rawDataVector.size();
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

    std::vector<zet_typed_value_t> interruptedExpectedMetricValues12 = {
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {2}},
        {ZET_VALUE_TYPE_UINT64, {4}},
        {ZET_VALUE_TYPE_UINT64, {5}},
        {ZET_VALUE_TYPE_UINT64, {6}},
        {ZET_VALUE_TYPE_UINT64, {7}},
        {ZET_VALUE_TYPE_UINT64, {8}},
        {ZET_VALUE_TYPE_UINT64, {9}},
        {ZET_VALUE_TYPE_UINT64, {10}},
        {ZET_VALUE_TYPE_UINT64, {3}}, // L0GfxCoreHelperHw arranges "OtherStall" to the end
        {ZET_VALUE_TYPE_UINT64, {10}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {40}},
        {ZET_VALUE_TYPE_UINT64, {50}},
        {ZET_VALUE_TYPE_UINT64, {60}},
        {ZET_VALUE_TYPE_UINT64, {70}},
        {ZET_VALUE_TYPE_UINT64, {80}},
        {ZET_VALUE_TYPE_UINT64, {90}},
        {ZET_VALUE_TYPE_UINT64, {100}},
        {ZET_VALUE_TYPE_UINT64, {30}} // L0GfxCoreHelperHw arranges "OtherStall" to the end
    };

    std::vector<zet_typed_value_t> interruptedExpectedMetricValues3 = {
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {9}},
        {ZET_VALUE_TYPE_UINT64, {7}},
        {ZET_VALUE_TYPE_UINT64, {6}},
        {ZET_VALUE_TYPE_UINT64, {5}},
        {ZET_VALUE_TYPE_UINT64, {4}},
        {ZET_VALUE_TYPE_UINT64, {3}},
        {ZET_VALUE_TYPE_UINT64, {2}},
        {ZET_VALUE_TYPE_UINT64, {1}},
        {ZET_VALUE_TYPE_UINT64, {8}} // L0GfxCoreHelperHw arranges "OtherStall" to the end
    };

    std::vector<zet_typed_value_t> interruptedExpectedMetricValues456 = {
        {ZET_VALUE_TYPE_UINT64, {10}}, // 4rd raw report
        {ZET_VALUE_TYPE_UINT64, {90}},
        {ZET_VALUE_TYPE_UINT64, {70}},
        {ZET_VALUE_TYPE_UINT64, {60}},
        {ZET_VALUE_TYPE_UINT64, {50}},
        {ZET_VALUE_TYPE_UINT64, {40}},
        {ZET_VALUE_TYPE_UINT64, {30}},
        {ZET_VALUE_TYPE_UINT64, {20}},
        {ZET_VALUE_TYPE_UINT64, {10}},
        {ZET_VALUE_TYPE_UINT64, {80}}, // L0GfxCoreHelperHw arranges "OtherStall" to the end
        {ZET_VALUE_TYPE_UINT64, {100}},
        {ZET_VALUE_TYPE_UINT64, {210}}, // 5th raw report + th raw report
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}}};

    std::vector<MockStallRawIpData> rawDataVectorOverflow = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},
                                                             {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},
                                                             {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x100}, // set the overflow bit in flags
                                                             {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3}};

    size_t rawDataVectorOverflowSize = sizeof(rawDataVectorOverflow[0]) * rawDataVectorOverflow.size();
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
