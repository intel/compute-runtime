/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"
#include <level_zero/zet_api.h>

#if defined(__cplusplus)
extern "C" {
#endif

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
    return L0::MetricGroup::fromHandle(hMetricGroup)->calculateMetricValuesExp(type, rawDataSize, pRawData, pSetCount, pTotalMetricValueCount, pMetricCounts, pMetricValues);
}

#if defined(__cplusplus)
} // extern "C"
#endif
