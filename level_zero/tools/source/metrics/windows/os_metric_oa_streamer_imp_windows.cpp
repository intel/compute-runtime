/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_streamer_imp.h"

namespace L0 {

uint32_t OaMetricStreamerImp::getOaBufferSize(const uint32_t notifyEveryNReports) const {

    // Notification is on half full buffer, hence multiplication by 2.
    return notifyEveryNReports * rawReportSize * 2;
}

} // namespace L0