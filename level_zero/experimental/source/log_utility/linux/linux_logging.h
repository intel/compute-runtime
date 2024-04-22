/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#define FMT_HEADER_ONLY

#include "level_zero/experimental/source/log_utility/logging.h"

namespace spdlog {
class logger;
}

namespace NEO {

class LinuxLogger : public Logger {
  public:
    LinuxLogger() {}
    ~LinuxLogger() override;
    int32_t createLogger(std::string loggerName, std::string filename, uint32_t logLevel) override;
    void setLevel(uint32_t logLevel) override;
    void logTrace(const char *msg) override;
    void logDebug(const char *msg) override;
    void logInfo(const char *msg) override;
    void logWarning(const char *msg) override;
    void logError(const char *msg) override;
    void logFatal(const char *msg) override;

  protected:
    MOCKABLE_VIRTUAL void setSpdLevel(uint32_t logLevel);
    MOCKABLE_VIRTUAL void logSpdTrace(const char *msg);
    MOCKABLE_VIRTUAL void logSpdDebug(const char *msg);
    MOCKABLE_VIRTUAL void logSpdInfo(const char *msg);
    MOCKABLE_VIRTUAL void logSpdWarning(const char *msg);
    MOCKABLE_VIRTUAL void logSpdError(const char *msg);
    MOCKABLE_VIRTUAL void logSpdFatal(const char *msg);
    MOCKABLE_VIRTUAL int32_t createSpdLogger(std::string loggerName, std::string filepath);
    std::shared_ptr<spdlog::logger> loggerObj = nullptr;

  private:
    std::string logFolderPath;
};

} // namespace NEO
