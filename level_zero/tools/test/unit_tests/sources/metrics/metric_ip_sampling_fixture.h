/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include <level_zero/zet_api.h>

namespace L0 {
namespace ult {
class MockMetricIpSamplingOsInterface;

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
class MetricIpSamplingFixture : public MultiDeviceFixture,
                                public ::testing::Test {
  public:
    void SetUp() override;
    void TearDown() override;

    std::vector<MockMetricIpSamplingOsInterface *> osInterfaceVector = {};
    std::vector<L0::Device *> testDevices = {};
};

class MetricIpSamplingCalculateMetricsFixture : public MetricIpSamplingFixture {
  public:
    std::vector<MockStallRawIpData> rawDataVector = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},
                                                     {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},
                                                     {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x000},
                                                     {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3}};

    size_t rawDataVectorSize = sizeof(rawDataVector[0]) * rawDataVector.size();
    std::vector<zet_typed_value_t> expectedMetricValues = {
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
};

} // namespace ult
} // namespace L0
