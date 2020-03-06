/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"

#if defined(_WIN64)
#define METRICS_DISCOVERY_NAME "igdmd64.dll"
#elif defined(_WIN32)
#define METRICS_DISCOVERY_NAME "igdmd32.dll"
#else
#error "Unsupported OS"
#endif

namespace L0 {

///////////////////////////////////////////////////////////////////////////////
/// @brief Returns Metrics Discovery library file name.
const char *MetricEnumeration::getMetricsDiscoveryFilename() { return METRICS_DISCOVERY_NAME; }

} // namespace L0
