/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

namespace L0 {

ze_result_t OaMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                       const uint8_t *pRawData, uint32_t *pSetCount,
                                                       uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                       zet_typed_value_t *pMetricValues) {
    return calculateMetricValuesImpl(type, rawDataSize, pRawData, pSetCount, pTotalMetricValueCount, pMetricCounts, pMetricValues);
}

} // namespace L0
