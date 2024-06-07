/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "linux_log_manager.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "driver_version.h"

#include <map>

#define QTR(a) #a
#define TOSTR(b) QTR(b)
#define LOG_INFO_VERSION_SHA(logType, version, sha)                                              \
    auto logger = NEO::LogManager::getInstance()->getLogger(logType);                            \
    if (logger) {                                                                                \
        char logBuffer[256];                                                                     \
        snprintf(logBuffer, 256, "Level zero driver version and SHA : %s - %s\n", version, sha); \
        logger->logInfo(logBuffer);                                                              \
    }

namespace NEO {
std::unique_ptr<LogManager> LogManager::instancePtr = nullptr;
uint32_t LogManager::logLevel = UINT32_MAX;

void initLogger() {
    if (NEO::LogManager::getLoggingLevel() != (uint32_t)NEO::LogLevel::logLevelOff) {
        CREATE_LOGGER(NEO::LogManager::LogType::coreLogger, "coreLogger.log", NEO::LogManager::getLoggingLevel());
        LOG_INFO_VERSION_SHA(NEO::LogManager::LogType::coreLogger, TOSTR(NEO_OCL_DRIVER_VERSION), NEO_REVISION);
    }
}

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

uint32_t LogManager::getLoggingLevel() {
    if (logLevel == UINT32_MAX) {
        logLevel = NEO::debugManager.flags.EnableLogLevel.get();
    }
    return logLevel;
}

std::unique_ptr<NEO::LinuxLogger> LinuxLogManager::prepareLogger() {
    return std::make_unique<NEO::LinuxLogger>();
}

LinuxLogManager::LinuxLogManager() {
}

} // namespace NEO
