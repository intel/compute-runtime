/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle_imp.h"
#include "level_zero/source/inc/ze_intel_gpu.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_query_imp.h"

#include "instrumentation.h"

#include <map>
#include <utility>

namespace L0 {

MetricQueryPool *MetricQueryPool::create(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup, const zet_metric_query_pool_desc_t &desc) { return nullptr; }
MetricQueryPool *MetricQueryPool::fromHandle(zet_metric_query_pool_handle_t handle) { return nullptr; }

ze_result_t metricQueryPoolCreate(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup, const zet_metric_query_pool_desc_t *pDesc,
                                  zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricQueryPoolDestroy(zet_metric_query_pool_handle_t hMetricQueryPool) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t MetricQuery::appendMemoryBarrier(CommandList &commandList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t MetricQuery::appendTracerMarker(CommandList &commandList,
                                            zet_metric_tracer_handle_t hMetricTracer, uint32_t value) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

MetricQuery *MetricQuery::fromHandle(zet_metric_query_handle_t handle) { return nullptr; }

void MetricContext::enableMetricApi(ze_result_t &result) {
    result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    return;
}

std::unique_ptr<MetricContext> MetricContext::create(Device &device) {
    return nullptr;
}

bool MetricContext::isMetricApiAvailable() {
    return false;
}

ze_result_t metricGroupGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricGet(zet_metric_group_handle_t hMetricGroup, uint32_t *pCount, zet_metric_handle_t *phMetrics) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricTracerOpen(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                             zet_metric_tracer_desc_t *pDesc, ze_event_handle_t hNotificationEvent,
                             zet_metric_tracer_handle_t *phMetricTracer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
