/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/kernel_dispatch_stats_reporter.h"

namespace L0 {

KernelDispatchStatsReporter::KernelDispatchStatsReporter(NEO::FileLogger<globalDebugFunctionalityLevel> &fileLoggerIn) : fileLogger(fileLoggerIn) {}

KernelDispatchStatsReporter::~KernelDispatchStatsReporter() {
    this->writeReport();
}

void KernelDispatchStatsReporter::writeReport() {
    NEO::writeKernelDispatchStatsReport(this->fileLogger, this->kernelDispatchStats);
}

KernelDispatchStatsReporter &kernelDispatchStatsReporter() {
    // constructed after the file logger, so it is destroyed and writes its final report before the file logger goes away
    static KernelDispatchStatsReporter kernelDispatchStatsReporter(NEO::fileLoggerInstance());
    return kernelDispatchStatsReporter;
}

} // namespace L0
