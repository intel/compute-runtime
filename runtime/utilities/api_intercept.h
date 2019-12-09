/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/perf_profiler.h"

#define API_ENTER(retValPointer) \
    DebugSettingsApiEnterWrapper<DebugManager.debugLoggingAvailable()> ApiWrapperForSingleCall(__FUNCTION__, retValPointer)
#define SYSTEM_ENTER()
#define SYSTEM_LEAVE(id)
#define WAIT_ENTER()
#define WAIT_LEAVE()

#if KMD_PROFILING == 1
#undef API_ENTER
#undef SYSTEM_ENTER
#undef SYSTEM_LEAVE
#undef WAIT_ENTER
#undef WAIT_LEAVE

#define API_ENTER(x) \
    PerfProfilerApiWrapper globalPerfProfilersWrapperInstanceForSingleApiFunction(__FUNCTION__)

#define SYSTEM_ENTER()      \
    PerfProfiler::create(); \
    gPerfProfiler->systemEnter();

#define SYSTEM_LEAVE(id) \
    gPerfProfiler->systemLeave(id);
#define WAIT_ENTER() \
    SYSTEM_ENTER()
#define WAIT_LEAVE() \
    SYSTEM_LEAVE(0)
#endif
