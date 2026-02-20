/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_test_hw_helper.h"

#include <array>
namespace L0 {
namespace ult {

void IpSamplingTestProductHelper::getMetricCount(PRODUCT_FAMILY productFamily, uint32_t &metricCount) {
    if (productFamily == IGFX_PVC) {
        metricCount = metricCountXe;
        return;
    } else if (productFamily >= IGFX_BMG) {
        metricCount = metricCountXe2AndLater;
        return;
    }
    metricCount = 0U;
}

void IpSamplingTestProductHelper::getMetricsProperties(PRODUCT_FAMILY productFamily, std::vector<IpSamplingTestProductHelper::MetricProperties> &metricsProperties) {
    if (productFamily == IGFX_PVC) {
        metricsProperties = metricsXe;
        return;
    } else if (productFamily >= IGFX_BMG) {
        metricsProperties = metricsXe2AndLater;
        return;
    }
    metricsProperties = {};
}

void IpSamplingTestProductHelper::rawElementsToRawReports(PRODUCT_FAMILY productFamily, bool overflow, std::vector<std::array<uint64_t, 8>> *rawReports) {
    if (productFamily == IGFX_PVC) {
        std::vector<RawReportElementsXe> rawDataElements = overflow ? rawDataElementsOverflowXe : rawDataElementsXe;
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
    } else if ((productFamily >= IGFX_BMG) && (productFamily <= IGFX_NVL_XE3G)) {
        for (uint32_t i = 0; i < rawDataElementsXe2AndLater.size(); i++) {
            std::array<uint64_t, 8> tempRawReport = {};
            tempRawReport[0] = (rawDataElementsXe2AndLater[i].ip & rawReportBitsXeToXe3.ipMask) |
                               ((rawDataElementsXe2AndLater[i].tdrCount & rawReportBitsXeToXe3.byteMask) << rawReportBitsXeToXe3.ipShift) |
                               ((rawDataElementsXe2AndLater[i].otherCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].controlCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + 2 * rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].pipeStallCount & rawReportBitsXeToXe3.byteMask) << (rawReportBitsXeToXe3.ipShift + 3 * rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].sendCount & 0x7) << (rawReportBitsXeToXe3.ipShift + 4 * rawReportBitsXeToXe3.byteShift));

            tempRawReport[1] = ((rawDataElementsXe2AndLater[i].sendCount & 0xf8) >> 3) |
                               ((rawDataElementsXe2AndLater[i].distAccCount & rawReportBitsXeToXe3.byteMask) << 5) |
                               ((rawDataElementsXe2AndLater[i].sbidCount & rawReportBitsXeToXe3.byteMask) << (5 + rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].syncCount & rawReportBitsXeToXe3.byteMask) << (5 + 2 * rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].instFetchCount & rawReportBitsXeToXe3.byteMask) << (5 + 3 * rawReportBitsXeToXe3.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].activeCount & rawReportBitsXeToXe3.byteMask) << (5 + 4 * rawReportBitsXeToXe3.byteShift));

            tempRawReport[2] = 0LL;
            tempRawReport[3] = 0LL;
            tempRawReport[4] = 0LL;
            tempRawReport[5] = 0LL;
            tempRawReport[6] = (rawDataElementsXe2AndLater[i].subSlice & rawReportBitsXeToXe3.wordMask) | ((rawDataElementsXe2AndLater[i].flags & rawReportBitsXeToXe3.wordMask) << rawReportBitsXeToXe3.wordShift);
            tempRawReport[7] = 0;
            (*rawReports).emplace_back(tempRawReport);
        }
        return;
    } else if (productFamily > IGFX_NVL_XE3G) {
        for (uint32_t i = 0; i < rawDataElementsXe2AndLater.size(); i++) {
            std::array<uint64_t, 8> tempRawReport = {};
            tempRawReport[0] = (rawDataElementsXe2AndLater[i].ip & rawReportBitsXe3pAndLater.ipMask) |
                               ((rawDataElementsXe2AndLater[i].tdrCount & 0x7) << rawReportBitsXe3pAndLater.ipShift);

            tempRawReport[1] = ((rawDataElementsXe2AndLater[i].tdrCount & 0xf8) >> 3) |
                               ((rawDataElementsXe2AndLater[i].otherCount & rawReportBitsXe3pAndLater.byteMask) << 5) |
                               ((rawDataElementsXe2AndLater[i].controlCount & rawReportBitsXe3pAndLater.byteMask) << (5 + rawReportBitsXe3pAndLater.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].pipeStallCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 2 * rawReportBitsXe3pAndLater.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].sendCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 3 * rawReportBitsXe3pAndLater.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].distAccCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 4 * rawReportBitsXe3pAndLater.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].sbidCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 5 * rawReportBitsXe3pAndLater.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].syncCount & rawReportBitsXe3pAndLater.byteMask) << (5 + 6 * rawReportBitsXe3pAndLater.byteShift)) |
                               ((rawDataElementsXe2AndLater[i].instFetchCount & 0x7) << (5 + 7 * rawReportBitsXe3pAndLater.byteShift));

            tempRawReport[2] = ((rawDataElementsXe2AndLater[i].instFetchCount & 0xf8) >> 3) |
                               ((rawDataElementsXe2AndLater[i].activeCount & rawReportBitsXe3pAndLater.byteMask) << 5);
            tempRawReport[3] = 0LL;
            tempRawReport[4] = 0LL;
            tempRawReport[5] = 0LL;
            tempRawReport[6] = (rawDataElementsXe2AndLater[i].subSlice & rawReportBitsXe3pAndLater.wordMask) | ((rawDataElementsXe2AndLater[i].flags & rawReportBitsXe3pAndLater.wordMask) << rawReportBitsXe3pAndLater.wordShift);
            tempRawReport[7] = 0;
            (*rawReports).emplace_back(tempRawReport);
        }
        return;
    }

    rawReports = nullptr;
}

void IpSamplingTestProductHelper::getExpectedCalculateResults(PRODUCT_FAMILY productFamily, std::vector<zet_typed_value_t> &expectedMetricValues) {
    if (productFamily == IGFX_PVC) {
        expectedMetricValues = expectedMetricValuesXe;
        return;
    } else if (productFamily >= IGFX_BMG) {
        expectedMetricValues = expectedMetricValuesXe2AndLater;
        return;
    }

    expectedMetricValues = {};
}

} // namespace ult
} // namespace L0
