/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "windows_log_manager.h"

namespace NEO {
std::unique_ptr<LogManager> LogManager::instancePtr = nullptr;
uint32_t LogManager::logLevel = UINT32_MAX;

LogManager *LogManager::getInstance() {
    if (instancePtr == nullptr) {
        LogManager::instancePtr = std::make_unique<WinLogManager>();
        return LogManager::instancePtr.get();
    } else {
        return LogManager::instancePtr.get();
    }
}

WinLogManager::~WinLogManager() {
}

void WinLogManager::createLogger(LogManager::LogType logType, std::string filename, uint32_t logLevel) {
}

NEO::Logger *WinLogManager::getLogger(LogManager::LogType logType) {
    return nullptr;
}

uint32_t LogManager::getLoggingLevel() {
    return (uint32_t)LogLevel::logLevelOff;
}

void WinLogManager::destroyLogger(LogManager::LogType logType) {
}

void initLogger(){};

} // namespace NEO
