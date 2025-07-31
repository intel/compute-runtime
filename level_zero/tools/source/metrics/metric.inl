/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

template <typename T>
ze_result_t MetricSource::activatePreferDeferredHierarchical(DeviceImp *deviceImp, const uint32_t count, zet_metric_group_handle_t *phMetricGroups) {
    for (auto &subDevice : deviceImp->subDevices) {
        T &source = subDevice->getMetricDeviceContext().getMetricSource<T>();
        auto status = source.activateMetricGroupsPreferDeferred(count, phMetricGroups);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0