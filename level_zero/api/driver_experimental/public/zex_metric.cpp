/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/zet_intel_gpu_metric.h"

namespace L0 {

ze_result_t ZE_APICALL zetIntelCommandListAppendMarkerExp(zet_command_list_handle_t hCommandList,
                                                          zet_metric_group_handle_t hMetricGroup,
                                                          uint32_t value) {

    auto metricGroupImp = static_cast<MetricGroupImp *>(L0::MetricGroup::fromHandle(hMetricGroup));
    return metricGroupImp->getMetricSource().appendMarker(hCommandList, hMetricGroup, value);
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL
zetIntelCommandListAppendMarkerExp(
    zet_command_list_handle_t hCommandList,
    zet_metric_group_handle_t hMetricGroup,
    uint32_t value) {
    return L0::zetIntelCommandListAppendMarkerExp(hCommandList, hMetricGroup, value);
}
}
