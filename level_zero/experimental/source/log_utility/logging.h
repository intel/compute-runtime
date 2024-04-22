/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <iostream>

namespace NEO {

class Logger {
  public:
    virtual int32_t createLogger(std::string loggerName, std::string filename, uint32_t logLevel) = 0;
    virtual void setLevel(uint32_t logLevel) = 0;
    virtual void logTrace(const char *msg) = 0;
    virtual void logDebug(const char *msg) = 0;
    virtual void logInfo(const char *msg) = 0;
    virtual void logWarning(const char *msg) = 0;
    virtual void logError(const char *msg) = 0;
    virtual void logFatal(const char *msg) = 0;
    virtual ~Logger() = default;
};

// Log levels
enum class LogLevel : uint32_t {
    logLevelTrace = 0,
    logLevelDebug = 1,
    logLevelInfo = 2,
    logLevelWarn = 3,
    logLevelError = 4,
    logLevelFatal = 5,
    logLevelOff = 6
};

// return codes
constexpr int logResultOk = 0;
constexpr int logResultError = -1;

} // namespace NEO
