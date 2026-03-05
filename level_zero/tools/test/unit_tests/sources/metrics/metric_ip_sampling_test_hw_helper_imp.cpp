/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_test_hw_helper.h"

#include "gtest/gtest.h"

#include <array>
#include <cstdio>
namespace L0 {
namespace ult {

void IpSamplingTestProductHelper::getExpectedMetricCount(PRODUCT_FAMILY productFamily, uint32_t &metricCount) {
    if (productFamily == IGFX_PVC) {
        metricCount = metricCountXe;
        return;
    } else if (productFamily >= IGFX_BMG) {
        metricCount = metricCountXe2AndLater;
        return;
    }
    metricCount = 0U;
}

void IpSamplingTestProductHelper::getExpectedMetricsProperties(PRODUCT_FAMILY productFamily, std::vector<IpSamplingTestProductHelper::MetricProperties> &metricsProperties) {
    if (productFamily == IGFX_PVC) {
        metricsProperties = metricsXe;
        return;
    } else if (productFamily >= IGFX_BMG) {
        metricsProperties = metricsXe2AndLater;
        return;
    }
    metricsProperties = {};
}

void IpSamplingTestProductHelper::rawElementsToRawReports(PRODUCT_FAMILY productFamily, InitReportType reportType, std::vector<std::array<uint64_t, 8>> *rawReports) {

    rawReports->clear();
    if (productFamily == IGFX_PVC) {
        std::vector<RawReportElementsXe> rawDataElements = (reportType == InitReportType::OverflowType) ? rawDataElementsOverflowXe : rawDataElementsXe;
        for (uint32_t i = 0; i < rawDataElements.size(); i++) {
            std::array<uint64_t, 8> tempRawReport = {};
            tempRawReport[0] = (rawDataElements[i].ip & rawReportBitsXeToXe3.ipMask) |
                               ((rawDataElements[i].activeCount & rawReportBitsXeToXe3.byteMask) << rawReportBitsXeToXe3.ipShift) |
                               ((rawDataElements[i].otherCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElements[i].controlCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + 2 * rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElements[i].pipeStallCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + 3 * rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElements[i].sendCount & 0x7) << (rawReportBitsXeToXe3.ipShift + 4 * rawReportBitsXeToXe3.byteShift));
            tempRawReport[1] = ((rawDataElements[i].sendCount & 0xf8) >> 3) |
                               ((rawDataElements[i].distAccCount & rawReportBitsXeToXe3.byteMask) << 5) |
                               ((rawDataElements[i].sbidCount & rawReportBitsXeToXe3.byteMask) << (5 + rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElements[i].syncCount & rawReportBitsXeToXe3.byteMask) << (5 + 2 * rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElements[i].instFetchCount & rawReportBitsXeToXe3.byteMask) << (5 + 3 * rawReportBitsXeToXe3.byteShift));
            tempRawReport[2] = 0LL;
            tempRawReport[3] = 0LL;
            tempRawReport[4] = 0LL;
            tempRawReport[5] = 0LL;
            tempRawReport[6] = (rawDataElements[i].subSlice & rawReportBitsXeToXe3.wordMask) | ((rawDataElements[i].flags & rawReportBitsXeToXe3.wordMask) << rawReportBitsXeToXe3.wordShift);
            tempRawReport[7] = 0;
            (*rawReports).emplace_back(tempRawReport);
        }
        return;
    } else if (productFamily >= IGFX_BMG) {

        std::vector<RawReportElementsXe2andLater> rawDataElements = {};
        switch (reportType) {
        case InitReportType::DefaultType:
            rawDataElements = rawDataElementsXe2AndLater;
            break;
        case InitReportType::SecondType:
            rawDataElements = secondRawDataElementsXe2AndLater;
            break;
        case InitReportType::AppendType:
            rawDataElements = rawDataElementsAppendXe2AndLater;
            break;
        default:
            break;
        }
        ASSERT_NE(0u, rawDataElements.size()) << "Invalid report type provided for the product family.";

        if (productFamily <= IGFX_NVL_XE3G) {
            for (uint32_t i = 0; i < rawDataElements.size(); i++) {
                std::array<uint64_t, 8> tempRawReport = {};
                tempRawReport[0] = (rawDataElements[i].ip & rawReportBitsXeToXe3.ipMask) |
                                   ((rawDataElements[i].tdrCount & rawReportBitsXeToXe3.byteMask) << rawReportBitsXeToXe3.ipShift) |
                                   ((rawDataElements[i].otherCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + rawReportBitsXeToXe3.byteShift)) |
                                   ((rawDataElements[i].controlCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + 2 * rawReportBitsXeToXe3.byteShift)) |
                                   ((rawDataElements[i].pipeStallCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + 3 * rawReportBitsXeToXe3.byteShift)) |
                                   ((rawDataElements[i].sendCount & 0x7) << (rawReportBitsXeToXe3.ipShift + 4 * rawReportBitsXeToXe3.byteShift));

                tempRawReport[1] = ((rawDataElements[i].sendCount & 0xf8) >> 3) |
                                   ((rawDataElements[i].distAccCount & rawReportBitsXeToXe3.byteMask) << 5) |
                                   ((rawDataElements[i].sbidCount & rawReportBitsXeToXe3.byteMask) << (5 + rawReportBitsXeToXe3.byteShift)) |
                                   ((rawDataElements[i].syncCount & rawReportBitsXeToXe3.byteMask) << (5 + 2 * rawReportBitsXeToXe3.byteShift)) |
                                   ((rawDataElements[i].instFetchCount & rawReportBitsXeToXe3.byteMask) << (5 + 3 * rawReportBitsXeToXe3.byteShift)) |
                                   ((rawDataElements[i].activeCount & rawReportBitsXeToXe3.byteMask) << (5 + 4 * rawReportBitsXeToXe3.byteShift));

                tempRawReport[2] = 0LL;
                tempRawReport[3] = 0LL;
                tempRawReport[4] = 0LL;
                tempRawReport[5] = 0LL;
                tempRawReport[6] = (rawDataElements[i].subSlice & rawReportBitsXeToXe3.wordMask) | ((rawDataElements[i].flags & rawReportBitsXeToXe3.wordMask) << rawReportBitsXeToXe3.wordShift);
                tempRawReport[7] = 0;
                (*rawReports).emplace_back(tempRawReport);
            }
            return;
        } else if (productFamily > IGFX_NVL_XE3G) {
            for (uint32_t i = 0; i < rawDataElements.size(); i++) {
                std::array<uint64_t, 8> tempRawReport = {};
                tempRawReport[0] = (rawDataElements[i].ip & rawReportBitsXe3pAndLater.ipMask) |
                                   ((rawDataElements[i].tdrCount & 0x7) << rawReportBitsXe3pAndLater.ipShift);

                tempRawReport[1] = ((rawDataElements[i].tdrCount & 0xf8) >> 3) |
                                   ((rawDataElements[i].otherCount & rawReportBitsXe3pAndLater.byteMask) << 5) |
                                   ((rawDataElements[i].controlCount & rawReportBitsXe3pAndLater.byteMask) << (5 + rawReportBitsXe3pAndLater.byteShift)) |
                                   ((rawDataElements[i].pipeStallCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 2 * rawReportBitsXe3pAndLater.byteShift)) |
                                   ((rawDataElements[i].sendCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 3 * rawReportBitsXe3pAndLater.byteShift)) |
                                   ((rawDataElements[i].distAccCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 4 * rawReportBitsXe3pAndLater.byteShift)) |
                                   ((rawDataElements[i].sbidCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 5 * rawReportBitsXe3pAndLater.byteShift)) |
                                   ((rawDataElements[i].syncCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 6 * rawReportBitsXe3pAndLater.byteShift)) |
                                   ((rawDataElements[i].instFetchCount & 0x7) << (5 + 7 * rawReportBitsXe3pAndLater.byteShift));

                tempRawReport[2] = ((rawDataElements[i].instFetchCount & 0xf8) >> 3) |
                                   ((rawDataElements[i].activeCount & rawReportBitsXe3pAndLater.byteMask) << 5);
                tempRawReport[3] = 0LL;
                tempRawReport[4] = 0LL;
                tempRawReport[5] = 0LL;
                tempRawReport[6] = (rawDataElements[i].subSlice & rawReportBitsXe3pAndLater.wordMask) | ((rawDataElements[i].flags & rawReportBitsXe3pAndLater.wordMask) << rawReportBitsXe3pAndLater.wordShift);
                tempRawReport[7] = 0;
                (*rawReports).emplace_back(tempRawReport);
            }
            return;
        }
    }

    rawReports = nullptr;
}

void IpSamplingTestProductHelper::getExpectedCalculateResults(PRODUCT_FAMILY productFamily, CalculationResultType resultsType, std::vector<uint64_t> &expectedMetricValues) {
    uint32_t metricCount = 0;
    getExpectedMetricCount(productFamily, metricCount);
    uint32_t scopesCount = 0;
    getExpectedRootDeviceMetricScopeCount(productFamily, scopesCount);
    std::vector<MetricProperties> expectedMetricsProperties = {};
    getExpectedMetricsProperties(productFamily, expectedMetricsProperties);

    expectedMetricValues.clear();

    // Three IPs, as defined by numperIpsInRawData: 1, 10 and 100
    if (resultsType == CalculationResultType::CompleteResults) {
        // IP 1, expected result is 11 for all metrics
        expectedMetricValues.push_back(1);
        expectedMetricValues.insert(expectedMetricValues.end(), metricCount - 1 /* don't count the IP */, 11);
        // IP 10, expected result is 110 for all metrics
        expectedMetricValues.push_back(10);
        expectedMetricValues.insert(expectedMetricValues.end(), metricCount - 1, 110);
        // IP 100, expected result is 210 for all metrics
        expectedMetricValues.push_back(100);
        expectedMetricValues.insert(expectedMetricValues.end(), metricCount - 1, 210);
    } else if (resultsType == CalculationResultType::AllScopesOddMetrics) {
        // result report will include only the odd metrics for all the scopes available
        if ((productFamily >= IGFX_PVC) && (productFamily <= IGFX_CRI)) {
            expectedMetricValues.insert(expectedMetricValues.end(), metricCount / 2 * scopesCount, 11);
            expectedMetricValues.insert(expectedMetricValues.end(), metricCount / 2 * scopesCount, 110);
            expectedMetricValues.insert(expectedMetricValues.end(), metricCount / 2 * scopesCount, 210);
        }
    } else if (resultsType == CalculationResultType::FirstRawReportOnly) {
        for (auto &metricProperty : expectedMetricsProperties) {
            if (productFamily == IGFX_PVC) {
                expectedMetricValues.push_back(firstRawReportXe[metricNameToRawElementIndexXe[metricProperty.name]]);
            } else if (productFamily >= IGFX_BMG) {
                expectedMetricValues.push_back(firstRawReportXe2AndLater[metricNameToRawElementIndexXe2AndLater[metricProperty.name]]);
            }
        }
    } else if (resultsType == CalculationResultType::ThirdRawReportOnly) {
        for (auto &metricProperty : expectedMetricsProperties) {
            if (productFamily == IGFX_PVC) {
                expectedMetricValues.push_back(thirdRawReportXe[metricNameToRawElementIndexXe[metricProperty.name]]);
            } else if (productFamily >= IGFX_BMG) {
                expectedMetricValues.push_back(thirdRawReportXe2AndLater[metricNameToRawElementIndexXe2AndLater[metricProperty.name]]);
            }
        }
    }
}

void IpSamplingTestProductHelper::getExpectedRootDeviceMetricScopeCount(PRODUCT_FAMILY productFamily, uint32_t &metricScopeCount) {
    if ((productFamily >= IGFX_PVC) && (productFamily <= IGFX_CRI)) {
        metricScopeCount = rootDevMetricScopesCountXe2Xe3p;
        return;
    }
    metricScopeCount = 0U;
}

void IpSamplingTestProductHelper::getExpectedRootDeviceMetricScopeProperties(PRODUCT_FAMILY productFamily, std::vector<zet_intel_metric_scope_properties_exp_t> &metricsScopesProperties) {
    if ((productFamily >= IGFX_PVC) && (productFamily <= IGFX_CRI)) {
        const std::string computeScopeName0 = std::string(computeScopeNamePrefix) + std::to_string(0);               // Sub-device 0
        const std::string computeScopeDescription0 = std::string(computeScopeDescriptionPrefix) + std::to_string(0); // Sub-device 0
        const std::string computeScopeName1 = std::string(computeScopeNamePrefix) + std::to_string(1);               // Sub-device 1
        const std::string computeScopeDescription1 = std::string(computeScopeDescriptionPrefix) + std::to_string(1); // Sub-device 1

        zet_intel_metric_scope_properties_exp_t metricScopePropertiesSubDevice0 = {ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP, nullptr, 0, {}, {}};
        zet_intel_metric_scope_properties_exp_t metricScopePropertiesSubDevice1 = {ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP, nullptr, 1, {}, {}};

        std::snprintf(metricScopePropertiesSubDevice0.name, sizeof(metricScopePropertiesSubDevice0.name), "%s", computeScopeName0.c_str());
        std::snprintf(metricScopePropertiesSubDevice0.description, sizeof(metricScopePropertiesSubDevice0.description), "%s", computeScopeDescription0.c_str());

        std::snprintf(metricScopePropertiesSubDevice1.name, sizeof(metricScopePropertiesSubDevice1.name), "%s", computeScopeName1.c_str());
        std::snprintf(metricScopePropertiesSubDevice1.description, sizeof(metricScopePropertiesSubDevice1.description), "%s", computeScopeDescription1.c_str());

        metricsScopesProperties = {metricScopePropertiesSubDevice0, metricScopePropertiesSubDevice1};
        return;
    }
    metricsScopesProperties = {};
}

} // namespace ult
} // namespace L0
