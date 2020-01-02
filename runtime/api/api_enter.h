/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/logger.h"
#include "runtime/utilities/perf_profiler.h"

#define API_ENTER(retValPointer) \
    LoggerApiEnterWrapper<NEO::FileLogger<globalDebugFunctionalityLevel>::enabled()> ApiWrapperForSingleCall(__FUNCTION__, retValPointer)

#if KMD_PROFILING == 1
#undef API_ENTER

#define API_ENTER(x) \
    PerfProfilerApiWrapper globalPerfProfilersWrapperInstanceForSingleApiFunction(__FUNCTION__)
#endif
