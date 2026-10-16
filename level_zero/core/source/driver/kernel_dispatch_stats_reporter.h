/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/kernel_dispatch_stats.h"
#include "shared/source/utilities/logger.h"

namespace L0 {

class KernelDispatchStatsReporter : NEO::NonCopyableAndNonMovableClass {
  public:
    KernelDispatchStatsReporter(NEO::FileLogger<globalDebugFunctionalityLevel> &fileLoggerIn);
    ~KernelDispatchStatsReporter();

    NEO::KernelDispatchStatsTracker &getKernelDispatchStats() { return this->kernelDispatchStats; }
    void writeReport();

  protected:
    NEO::FileLogger<globalDebugFunctionalityLevel> &fileLogger;
    NEO::KernelDispatchStatsTracker kernelDispatchStats;
};

static_assert(NEO::NonCopyableAndNonMovable<KernelDispatchStatsReporter>);

KernelDispatchStatsReporter &kernelDispatchStatsReporter();

} // namespace L0
