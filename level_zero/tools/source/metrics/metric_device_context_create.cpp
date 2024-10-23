/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_programmable_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

namespace L0 {

std::unique_ptr<MetricDeviceContext> MetricDeviceContext::create(Device &device) {
    return std::make_unique<MetricDeviceContext>(device);
}

} // namespace L0