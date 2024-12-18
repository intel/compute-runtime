/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/utilities/logger_tests.h"

#include "opencl/source/utilities/cl_logger.h"

#include <map>

template <DebugFunctionalityLevel debugLevel>
class TestClFileLogger : public NEO::ClFileLogger<debugLevel> {
  public:
    TestClFileLogger(TestFileLogger<debugLevel> &baseLoggerInm, const NEO::DebugVariables &flags)
        : NEO::ClFileLogger<debugLevel>(baseLoggerInm, flags), baseLogger(baseLoggerInm) { baseLogger.useRealFiles(false); }

  protected:
    TestFileLogger<debugLevel> &baseLogger;
};

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

using FullyEnabledClFileLogger = TestClFileLogger<DebugFunctionalityLevel::full>;
using FullyDisabledClFileLogger = TestClFileLogger<DebugFunctionalityLevel::none>;