/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

#if defined(_WIN64)
#define METRICS_DISCOVERY_NAME "igdmd64.dll"
#elif defined(_WIN32)
#define METRICS_DISCOVERY_NAME "igdmd32.dll"
#else
#error "Unsupported OS"
#endif

namespace L0 {

void MetricEnumeration::getMetricsDiscoveryFilename(std::vector<const char *> &names) const {
    names.clear();
    names.push_back(METRICS_DISCOVERY_NAME);
}

} // namespace L0
