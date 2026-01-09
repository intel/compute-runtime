/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

template <typename T>
ze_result_t MetricSource::activatePreferDeferredHierarchical(Device *device, const uint32_t count, zet_metric_group_handle_t *phMetricGroups) {
    for (auto &subDevice : device->subDevices) {
        T &source = subDevice->getMetricDeviceContext().getMetricSource<T>();
        auto status = source.activateMetricGroupsPreferDeferred(count, phMetricGroups);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0