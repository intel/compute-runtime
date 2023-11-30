/*
 * Copyright (C) 2019-2023 Intel Corporation
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

template <bool debugFunctionality>
class TestLoggerApiEnterWrapper : public NEO::LoggerApiEnterWrapper<debugFunctionality> {
  public:
    TestLoggerApiEnterWrapper(const char *functionName, int *errCode) : NEO::LoggerApiEnterWrapper<debugFunctionality>(functionName, errCode) {
        if (debugFunctionality) {
            loggedEnter = true;
        }
    }

    bool loggedEnter = false;
};
