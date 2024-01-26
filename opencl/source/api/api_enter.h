/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/logger.h"
#include "shared/source/utilities/perf_profiler.h"

#define API_ENTER(retValPointer) \
    LoggerApiEnterWrapper<NEO::FileLogger<globalDebugFunctionalityLevel>::enabled()> ApiWrapperForSingleCall(__FUNCTION__, retValPointer)
