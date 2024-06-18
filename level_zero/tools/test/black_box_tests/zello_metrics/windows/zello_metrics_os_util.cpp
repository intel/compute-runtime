/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

namespace ZelloMetricsUtility {

bool osStreamMpCollectionWorkloadDifferentProcess() {
    bool status = true;
    LOG(LogLevel::INFO) << " : streamMpCollectionWorkloadDifferentProcess test not supported on windows.\n";
    return status;
}

bool osStreamMpCollectionWorkloadSameProcess() {
    bool status = true;
    LOG(LogLevel::INFO) << " : streamMpCollectionWorkloadSameProcess test not supported on windows.\n";
    return status;
}

int32_t osRunAllTests(int32_t runStatus) {
    LOG(LogLevel::INFO) << " : all test not supported on windows. Please run tests individually.\n";
    return runStatus;
}

} // namespace ZelloMetricsUtility
