/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/preprocessor.h"

#include "opencl/source/utilities/cl_logger.h"

#define API_ENTER(retValPointer) \
    LoggerApiEnterWrapper<NEO::FileLogger<globalDebugFunctionalityLevel>::enabled()> ApiWrapperForSingleCall(NEO_FUNCTION_NAME, retValPointer)
