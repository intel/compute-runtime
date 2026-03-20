/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_test_hw_helper.h"
namespace L0 {
namespace ult {

IpSamplingTestProductHelper *IpSamplingTestProductHelper::create() {
    return new IpSamplingTestProductHelper();
}

} // namespace ult
} // namespace L0
