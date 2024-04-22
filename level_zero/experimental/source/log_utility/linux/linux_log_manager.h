/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/experimental/source/log_utility/log_manager.h"

#include "linux_logging.h"

#include <map>

namespace NEO {

class LinuxLogManager : public LogManager {
  public:
    std::string getLoggerName(LogManager::LogType logType);

    void createLogger(LogManager::LogType logType, std::string filename, uint32_t logLevel) override;

    NEO::Logger *getLogger(LogManager::LogType logType) override;

    void destroyLogger(LogManager::LogType logType) override;

    MOCKABLE_VIRTUAL std::unique_ptr<NEO::LinuxLogger> createLoggerByType(LogManager::LogType logType, std::string filename, uint32_t logLevel);

    MOCKABLE_VIRTUAL std::unique_ptr<NEO::LinuxLogger> prepareLogger();

    LinuxLogManager();
    ~LinuxLogManager() override;

  protected:
    std::map<LogManager::LogType, std::unique_ptr<NEO::Logger>> loggerMap;
};

} // namespace NEO
