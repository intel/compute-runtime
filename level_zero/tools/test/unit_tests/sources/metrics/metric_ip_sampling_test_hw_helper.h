/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/include/level_zero/zet_intel_gpu_metric.h"
#include "level_zero/tools/source/metrics/metric.h"
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

    enum CalculationResultType {
        CompleteResults = 0,     // Expect all results from the raw reports
        AllScopesOddMetrics = 1, // zetIntelMetricCalculateValuesExp() with odd metrics count in the calcOp
        FirstRawReportOnly = 2,  // Expect results only for the first raw report
        ThirdRawReportOnly = 3,  // Expect results only for the third raw report
    };

    enum InitReportType {
        DefaultType = 0,  // Initialize raw data elements with default values
        OverflowType = 1, // Initialize raw data elements with values that would cause overflow in metric calculation
        SecondType = 2,   // Initialize raw data elements with second type values
        AppendType = 3    // Initialize raw data elements with values that should be appended to already read data
    };

    static IpSamplingTestProductHelper *create();
    virtual void getExpectedMetricCount(PRODUCT_FAMILY productFamily, uint32_t &metricCount);
    virtual void getExpectedMetricsProperties(PRODUCT_FAMILY productFamily, std::vector<MetricProperties> &metricsProperties);
    virtual void rawElementsToRawReports(PRODUCT_FAMILY productFamily, InitReportType reportType, std::vector<std::array<uint64_t, 8>> *rawReports); // Raw reports are 64Bytes, 8 x uint64_t
    virtual void getExpectedCalculateResults(PRODUCT_FAMILY productFamily, CalculationResultType resultsType, std::vector<uint64_t> &expectedMetricValues);
    virtual void getExpectedRootDeviceMetricScopeCount(PRODUCT_FAMILY productFamily, uint32_t &metricScopeCount);
    virtual void getExpectedRootDeviceMetricScopeProperties(PRODUCT_FAMILY productFamily, std::vector<zet_intel_metric_scope_properties_exp_t> &metricsScopesProperties);

    // Define the number of IPs to have in manually constructed raw data elements
    constexpr static uint32_t numberOfIpsInRawData = 3;

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
        {"OtherStall", "Stall on other condition", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"}};

    std::map<std::string, uint32_t> metricNameToRawElementIndexXe = { // see RawReportElementsXe
        {"IP", 0},
        {"Active", 1},
        {"ControlStall", 3},
        {"PipeStall", 4},
        {"SendStall", 5},
        {"DistStall", 6},
        {"SbidStall", 7},
        {"SyncStall", 8},
        {"InstrFetchStall", 9},
        {"OtherStall", 2}};

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
        {"OtherStall", "Stall on other condition", "XVE", 4, ZET_METRIC_TYPE_EVENT, ZET_VALUE_TYPE_UINT64, "Events"}};

    std::map<std::string, uint32_t> metricNameToRawElementIndexXe2AndLater = { // see RawReportElementsXe2andLater
        {"IP", 0},
        {"Active", 2},
        {"PSDepStall", 1},
        {"ControlStall", 4},
        {"PipeStall", 5},
        {"SendStall", 6},
        {"DistStall", 7},
        {"SbidStall", 8},
        {"SyncStall", 9},
        {"InstrFetchStall", 10},
        {"OtherStall", 3}};

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
        uint64_t tdrCount; // PSDepStall for Xe2 and later
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

    // IP = 1, Active = 2, Other = 3, Control = 4, PipeStall = 5, SendStall = 6, DistStall = 7,
    // SbidStall = 8, SyncStall = 9, InstrFetchStall = 10, SubSlice = 1000, Flags = 0x01
    std::vector<uint64_t> firstRawReportXe = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01};

    // IP = 1, Active = 9, Other = 8, Control = 7, PipeStall = 6, SendStall = 5, DistStall = 4,
    // SbidStall = 3, SyncStall = 2, InstrFetchStall = 1, SubSlice = 1000, Flags = 0x02
    std::vector<uint64_t> thirdRawReportXe = {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02};

    std::vector<RawReportElementsXe> rawDataElementsXe = {
        {firstRawReportXe[0], firstRawReportXe[1], firstRawReportXe[2], firstRawReportXe[3],
         firstRawReportXe[4], firstRawReportXe[5], firstRawReportXe[6], firstRawReportXe[7],
         firstRawReportXe[8], firstRawReportXe[9], firstRawReportXe[10], firstRawReportXe[11]}, // 1st raw report
        {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x000},                                 // 2nd raw report
        {thirdRawReportXe[0], thirdRawReportXe[1], thirdRawReportXe[2], thirdRawReportXe[3],
         thirdRawReportXe[4], thirdRawReportXe[5], thirdRawReportXe[6], thirdRawReportXe[7],
         thirdRawReportXe[8], thirdRawReportXe[9], thirdRawReportXe[10], thirdRawReportXe[11]}, // 3rd raw report
        {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},                                    // 4th raw report
        {100, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3},                          // 5th raw report
        {100, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};                                 // 6th raw report

    std::vector<RawReportElementsXe> rawDataElementsOverflowXe = {
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01},                   // 1st raw report
        {1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},                    // 2nd raw report
        {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x100},        // set the overflow bit in flags 3rd raw report
        {10, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},           // 4th raw report
        {100, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3}, // 5th raw report
        {100, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};        // 6th raw report

    // IP = 1, TDR(PSDepStall) = 1, Active = 2, Other = 3, Control = 4, PipeStall = 5, SendStall = 6, DistStall = 7,
    // SbidStall = 8, SyncStall = 9, InstrFetchStall = 10, SubSlice = 1000, Flags = 0x01
    std::vector<uint64_t> firstRawReportXe2AndLater = {1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1000, 0x01};

    // IP = 1, TDR(PSDepStall) = 10, Active = 9, Other = 8, Control = 7, PipeStall = 6, SendStall = 5, DistStall = 4,
    // SbidStall = 3, SyncStall = 2, InstrFetchStall = 1, SubSlice = 1000, Flags = 0x02
    std::vector<uint64_t> thirdRawReportXe2AndLater = {1, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02};

    std::vector<RawReportElementsXe2andLater> rawDataElementsXe2AndLater = {
        {firstRawReportXe2AndLater[0], firstRawReportXe2AndLater[1], firstRawReportXe2AndLater[2], firstRawReportXe2AndLater[3],
         firstRawReportXe2AndLater[4], firstRawReportXe2AndLater[5], firstRawReportXe2AndLater[6], firstRawReportXe2AndLater[7],
         firstRawReportXe2AndLater[8], firstRawReportXe2AndLater[9], firstRawReportXe2AndLater[10], firstRawReportXe2AndLater[11],
         firstRawReportXe2AndLater[12]},                            // 1st raw report
        {10, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1001, 0x000}, // 2nd raw report
        {thirdRawReportXe2AndLater[0], thirdRawReportXe2AndLater[1], thirdRawReportXe2AndLater[2], thirdRawReportXe2AndLater[3],
         thirdRawReportXe2AndLater[4], thirdRawReportXe2AndLater[5], thirdRawReportXe2AndLater[6], thirdRawReportXe2AndLater[7],
         thirdRawReportXe2AndLater[8], thirdRawReportXe2AndLater[9], thirdRawReportXe2AndLater[10], thirdRawReportXe2AndLater[11],
         thirdRawReportXe2AndLater[12]},                                    // 3rd raw report
        {10, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 1000, 0x3},           // 4th raw report
        {100, 200, 190, 180, 170, 160, 150, 140, 130, 120, 110, 1000, 0x3}, // 5th raw report
        {100, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1000, 0x3}};         // 6th raw report

    std::vector<RawReportElementsXe2andLater> secondRawDataElementsXe2AndLater = {
        {2, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 1000, 0x01},  // 1st raw report: new IP and values
        {100, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 1000, 0x3}, // 2nd raw report: repeated IP and new values from rawDataElements
        {2, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1000, 0x02},           // 3rd raw report
        {100, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1000, 0x3}};          // 4th raw report

    std::vector<RawReportElementsXe2andLater> rawDataElementsAppendXe2AndLater = {
        {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 1000, 0x01},         // 1st raw report: new IP and values
        {100, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 1000, 0x3}}; // 2nd raw report: repeated IP and new values from rawDataElements

    uint32_t rootDevMetricScopesCountXe2Xe3p = 2; // Sub-device 0 and Sub-device 1
};

} // namespace ult
} // namespace L0
