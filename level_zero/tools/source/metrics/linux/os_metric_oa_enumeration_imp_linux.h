/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/metrics/os_interface_metric.h"

#include "metrics_discovery_api.h"

namespace L0 {
struct Device;
struct MetricEnumeration;

bool getDrmAdapterId(uint32_t &adapterMajor, uint32_t &adapterMinor, Device &device);

MetricsDiscovery::IAdapter_1_13 *getDrmMetricsAdapter(MetricEnumeration *metricEnumeration);
} // namespace L0
