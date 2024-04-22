/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "linux_log_manager.h"

#include <map>

namespace NEO {
std::unique_ptr<LogManager> LogManager::instancePtr = nullptr;

LogManager *LogManager::getInstance() {
    if (instancePtr == nullptr) {
        LogManager::instancePtr = std::make_unique<LinuxLogManager>();
        return LogManager::instancePtr.get();
    } else {
        return LogManager::instancePtr.get();
    }
}

LinuxLogManager::~LinuxLogManager() {
}

std::string LinuxLogManager::getLoggerName(LogManager::LogType logType) {
    if (logType == LogType::coreLogger) {
        return "coreLogger";
    }
    return "";
}

void LinuxLogManager::createLogger(LogManager::LogType logType, std::string filename, uint32_t logLevel) {
    auto logger = createLoggerByType(logType, filename, logLevel);
    if (logger != nullptr) {
        loggerMap.insert({logType, std::move(logger)});
    }
}

NEO::Logger *LinuxLogManager::getLogger(LogManager::LogType logType) {
    if (loggerMap.find(logType) != loggerMap.end()) {
        return loggerMap[logType].get();
    }
    return nullptr;
}

void LinuxLogManager::destroyLogger(LogManager::LogType logType) {
    loggerMap.erase(logType);
}

std::unique_ptr<NEO::LinuxLogger> LinuxLogManager::createLoggerByType(LogManager::LogType logType, std::string filename, uint32_t logLevel) {
    std::unique_ptr<NEO::LinuxLogger> pLogger = prepareLogger();
    int32_t ret = pLogger->createLogger(getLoggerName(logType), filename, logLevel);
    if (ret < 0) {
        pLogger.reset();
        return nullptr;
    }
    return pLogger;
}

std::unique_ptr<NEO::LinuxLogger> LinuxLogManager::prepareLogger() {
    return std::make_unique<NEO::LinuxLogger>();
}

LinuxLogManager::LinuxLogManager() {
}

} // namespace NEO
