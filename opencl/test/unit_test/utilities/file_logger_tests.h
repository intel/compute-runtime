/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/utilities/directory.h"

#include "opencl/source/utilities/cl_logger.h"

#include <map>

using FullyEnabledClFileLogger = NEO::ClFileLogger<DebugFunctionalityLevel::Full>;
using FullyDisabledClFileLogger = NEO::ClFileLogger<DebugFunctionalityLevel::None>;

template <bool DebugFunctionality>
class TestLoggerApiEnterWrapper : public NEO::LoggerApiEnterWrapper<DebugFunctionality> {
  public:
    TestLoggerApiEnterWrapper(const char *functionName, int *errCode) : NEO::LoggerApiEnterWrapper<DebugFunctionality>(functionName, errCode) {
        if (DebugFunctionality) {
            loggedEnter = true;
        }
    }

    bool loggedEnter = false;
};
