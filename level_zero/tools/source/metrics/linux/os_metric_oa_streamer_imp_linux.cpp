/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_streamer_imp.h"

namespace L0 {

uint32_t OaMetricStreamerImp::getOaBufferSize(const uint32_t notifyEveryNReports) const {

    auto metricGroup = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup));
    const auto maxOaBufferSize = metricGroup->getMetricEnumeration().getMaxOaBufferSize();

    // if maxOaBufferSize reading failed, use Notification on half full buffer approach.
    return std::max(maxOaBufferSize, notifyEveryNReports * rawReportSize * 2);
}

} // namespace L0