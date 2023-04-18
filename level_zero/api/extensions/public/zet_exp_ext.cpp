/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"
#include <level_zero/zet_api.h>

namespace L0 {

ze_result_t zetMetricGroupCalculateMultipleMetricValuesExp(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_calculation_type_t type,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pSetCount,
    uint32_t *pTotalMetricValueCount,
    uint32_t *pMetricCounts,
    zet_typed_value_t *pMetricValues) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->calculateMetricValuesExp(type, rawDataSize, pRawData, pSetCount, pTotalMetricValueCount, pMetricCounts, pMetricValues);
}

ze_result_t zetMetricGroupGetGlobalTimestampsExp(
    zet_metric_group_handle_t hMetricGroup,
    ze_bool_t synchronizedWithHost,
    uint64_t *globalTimestamp,
    uint64_t *metricTimestamp) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->getMetricTimestampsExp(synchronizedWithHost, globalTimestamp, metricTimestamp);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupCalculateMultipleMetricValuesExp(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_calculation_type_t type,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pSetCount,
    uint32_t *pTotalMetricValueCount,
    uint32_t *pMetricCounts,
    zet_typed_value_t *pMetricValues) {
    return L0::zetMetricGroupCalculateMultipleMetricValuesExp(hMetricGroup, type, rawDataSize, pRawData, pSetCount, pTotalMetricValueCount, pMetricCounts, pMetricValues);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricGroupGetGlobalTimestampsExp(
    zet_metric_group_handle_t hMetricGroup,
    ze_bool_t synchronizedWithHost,
    uint64_t *globalTimestamp,
    uint64_t *metricTimestamp) {
    return L0::zetMetricGroupGetGlobalTimestampsExp(hMetricGroup, synchronizedWithHost, globalTimestamp, metricTimestamp);
}

} // extern "C"
