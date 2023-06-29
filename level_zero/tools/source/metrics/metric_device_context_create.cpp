/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

std::unique_ptr<MetricDeviceContext> MetricDeviceContext::create(Device &device) {
    return std::make_unique<MetricDeviceContext>(device);
}

} // namespace L0