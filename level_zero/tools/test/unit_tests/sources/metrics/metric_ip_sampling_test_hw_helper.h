/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "neo_igfxfmid.h"

#include <memory>
#include <stdexcept>
#include <vector>

namespace L0 {
namespace ult {

class IpSamplingTestProductHelper {
  public:
    IpSamplingTestProductHelper() = default;
    virtual ~IpSamplingTestProductHelper() = default;

    struct MetricProperties {
        const char *name;
        const char *description;
        const char *component;
        uint32_t tierNumber;
        zet_metric_type_t metricType;
        zet_value_type_t resultType;
        const char *resultUnits;
    };

    static IpSamplingTestProductHelper *create();
    virtual void getMetricCount(PRODUCT_FAMILY productFamily, uint32_t &metricCount);
    virtual void getMetricsProperties(PRODUCT_FAMILY productFamily, std::vector<MetricProperties> &metricsProperties);
    virtual void rawElementsToRawReports(PRODUCT_FAMILY productFamily, bool overflow, std::vector<std::array<uint64_t, 8>> *rawReports); // Raw reports are 64Bytes, 8 x uint64_t
    virtual void getExpectedCalculateResults(PRODUCT_FAMILY productFamily, std::vector<zet_typed_value_t> &expectedMetricValues);

    // Define the number of IPs to have in manually constructed raw data elements
    constexpr static uint32_t numperIpsInRawData = 3;

  private:
    uint32_t metricCountXe = 10;
    uint32_t metricCountXe2AndLater = 11;

    std::vector<MetricProperties> metricsXe = {
        {"IP", "IP address", "XVE", 4, ZET_METRIC_TYPE_IP, ZET_VALUE_TYPE_UINT64, "Address"},
        {"Active", "Active cycles", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"ControlStall", "Stall on control", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"PipeStall", "Stall on pipe", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SendStall", "Stall on send", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"DistStall", "Stall on distance", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SbidStall", "Stall on scoreboard", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SyncStall", "Stall on sync", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"InstrFetchStall", "Stall on instruction fetch", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"OtherStall", "Stall on other condition", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
    };

    std::vector<MetricProperties> metricsXe2AndLater = {
        {"IP", "IP address", "XVE", 4, ZET_METRIC_TYPE_IP, ZET_VALUE_TYPE_UINT64, "Address"},
        {"Active", "Active cycles", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"PSDepStall", "Stall on Pixel Shader Order Dependency", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"ControlStall", "Stall on control", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"PipeStall", "Stall on pipe", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SendStall", "Stall on send", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"DistStall", "Stall on distance", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SbidStall", "Stall on scoreboard", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"SyncStall", "Stall on sync", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"InstrFetchStall", "Stall on instruction fetch", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
        {"OtherStall", "Stall on other condition", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"},
    };
    struct RawReportBits {
        uint64_t ipShift;
        uint64_t ipMask;
        uint64_t byteShift;
        uint64_t byteMask;
        uint64_t wordShift;
        uint64_t wordMask;
    };
    struct RawReportElementsXe {
        uint64_t ip;
        uint64_t activeCount;
        uint64_t otherCount;
        uint64_t controlCount;
        uint64_t pipeStallCount;
        uint64_t sendCount;
        uint64_t distAccCount;
        uint64_t sbidCount;
        uint64_t syncCount;
        uint64_t instFetchCount;
        uint64_t subSlice;
        uint64_t flags;
    };
    struct RawReportElementsXe2andLater {
        uint64_t ip;
        uint64_t tdrCount;
        uint64_t activeCount;
        uint64_t otherCount;
        uint64_t controlCount;
        uint64_t pipeStallCount;
        uint64_t sendCount;
        uint64_t distAccCount;
        uint64_t sbidCount;
        uint64_t syncCount;
        uint64_t instFetchCount;
        uint64_t subSlice;
        uint64_t flags;
    };

    struct RawReportBits rawReportBitsXeToXe3 = {29, 0x1fffffff, 8, 0xff, 16, 0xffff};
    struct RawReportBits rawReportBitsXe3pAndLater = {61, 0x1fffffffffffffff, 8, 0xff, 16, 0xffff};

    std::vector<RawReportElementsXe> rawDataElementsXe = {
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},                   // 1st raw report
        {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x000},        // 2nd raw report
        {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},                    // 3rd raw report
        {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},           // 4th raw report
        {100, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3}, // 5th raw report
        {100, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};        // 6th raw report

    std::vector<RawReportElementsXe> rawDataElementsOverflowXe = {
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},                   // 1st raw report
        {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},                    // 2nd raw report
        {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x100},        // set the overflow bit in flags 3rd raw report
        {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},           // 4th raw report
        {100, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3}, // 5th raw report
        {100, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};        // 6th raw report

    std::vector<RawReportElementsXe2andLater> rawDataElementsXe2AndLater = {
        {1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},                   // 1st raw report
        {10, 1, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x000},        // 2nd raw report
        {1, 1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},                    // 3rd raw report
        {10, 1, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},           // 4th raw report
        {100, 1, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3}, // 5th raw report
        {100, 1, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};        // 6th raw report

    std::vector<zet_typed_value_t> expectedMetricValuesXe = {
        {ZET_VALUE_TYPE_UINT64, {1}}, // 1st raw report + 3rd raw report
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {10}}, // 2nd raw report + 4th raw report
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {100}}, // 5th raw report + 6th raw report
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}}};

    std::vector<zet_typed_value_t> expectedMetricValuesXe2AndLater = {
        {ZET_VALUE_TYPE_UINT64, {1}}, // 1st raw report + 3rd raw report
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {2}}, // PSDepStall count from 1st raw report + 3rd raw report
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {11}},
        {ZET_VALUE_TYPE_UINT64, {10}}, // 2nd raw report + 4th raw report
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {2}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {110}},
        {ZET_VALUE_TYPE_UINT64, {100}}, // 5th raw report + 6th raw report
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {2}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}},
        {ZET_VALUE_TYPE_UINT64, {210}}};
};

} // namespace ult
} // namespace L0
