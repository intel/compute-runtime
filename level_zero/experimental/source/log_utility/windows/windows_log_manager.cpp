/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "windows_log_manager.h"

namespace NEO {
std::unique_ptr<LogManager> LogManager::instancePtr = nullptr;

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

void WinLogManager::destroyLogger(LogManager::LogType logType) {
}

} // namespace NEO
