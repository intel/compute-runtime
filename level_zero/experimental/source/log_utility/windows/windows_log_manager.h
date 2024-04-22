/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/experimental/source/log_utility/log_manager.h"

namespace NEO {

class WinLogManager : public LogManager {
  public:
    virtual void createLogger(LogManager::LogType logType, std::string filename, uint32_t logLevel) override;

    virtual NEO::Logger *getLogger(LogManager::LogType logType) override;

    virtual void destroyLogger(LogManager::LogType logType) override;

    ~WinLogManager() override;
};

} // namespace NEO
