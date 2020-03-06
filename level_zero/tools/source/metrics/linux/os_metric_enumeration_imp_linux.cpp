/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"

namespace L0 {

///////////////////////////////////////////////////////////////////////////////
/// @brief Returns Metrics Discovery library file name.
const char *MetricEnumeration::getMetricsDiscoveryFilename() { return "libmd.so"; }

} // namespace L0
