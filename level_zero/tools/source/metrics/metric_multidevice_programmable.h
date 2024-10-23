/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

struct MultiDeviceCreatedMetricGroupManager {

    MultiDeviceCreatedMetricGroupManager(MetricSource &metricSource,
                                         std::vector<MetricGroupImp *> &subDeviceMetricGroupsCreated,
                                         std::vector<MultiDeviceMetricImp *> &multiDeviceMetrics);
    template <typename T>
    static ze_result_t createMultipleMetricGroupsFromMetrics(const MetricDeviceContext &metricDeviceContext,
                                                             MetricSource &metricSource,
                                                             std::vector<zet_metric_handle_t> &metricList,
                                                             const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                                             const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                             uint32_t *maxMetricGroupCount,
                                                             std::vector<zet_metric_group_handle_t> &metricGroupList);
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics);
    ze_result_t addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString);
    ze_result_t removeMetric(zet_metric_handle_t hMetric);
    ze_result_t close();
    ze_result_t destroy();
    void deleteMetricAddedDuringClose(Metric *metric);

  protected:
    MetricSource &metricSource;
    std::vector<MetricGroupImp *> &subDeviceMetricGroupsCreated;
    std::vector<MultiDeviceMetricImp *> &multiDeviceMetrics;
};

} // namespace L0