/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {
namespace ult {

class MockMetricSource : public L0::MetricSource {
  public:
    void enable() override {}
    bool isAvailable() override { return false; }
    ze_result_t appendMetricMemoryBarrier(L0::CommandList &commandList) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t getTimerResolution(uint64_t &resolution) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t getTimestampValidBits(uint64_t &validBits) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t activateMetricGroupsPreferDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) override { return ZE_RESULT_ERROR_UNKNOWN; }
    ze_result_t activateMetricGroupsAlreadyDeferred() override { return ZE_RESULT_ERROR_UNKNOWN; }
    ~MockMetricSource() override = default;
};

} // namespace ult
} // namespace L0
