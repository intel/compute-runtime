/*
 * Copyright (C) 2024 Intel Corporation
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

class MetricOALinuxImp : public MetricOAOsInterface {
  public:
    MetricOALinuxImp(Device &device);
    ~MetricOALinuxImp() override = default;
    ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) override;

  private:
    Device &device;
};

} // namespace L0
