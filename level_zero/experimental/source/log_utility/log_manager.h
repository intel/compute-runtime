/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "logging.h"

#include <memory>

#if defined(__GNUC__) || defined(__clang__)
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#else // defined(__GNUC__) || defined(__clang__)
#ifndef unlikely
#define unlikely(x) (x)
#endif
#endif // defined(__GNUC__) || defined(__clang__)

namespace NEO {

class LogManager {
  public:
    enum class LogType : uint32_t {
        coreLogger,
        logMax // Must be last
    };

    static LogManager *getInstance();

    virtual void createLogger(LogManager::LogType logType, std::string filename, uint32_t logLevel) = 0;

    virtual NEO::Logger *getLogger(LogManager::LogType logType) = 0;

    virtual void destroyLogger(LogManager::LogType logType) = 0;

    static uint32_t getLoggingLevel();

    virtual ~LogManager() = default;

  protected:
    static std::unique_ptr<LogManager> instancePtr;
    static uint32_t logLevel;
};

#define CREATE_LOGGER(logType, filename, logLevel) NEO::LogManager::getInstance()->createLogger(logType, filename, logLevel)

#define LOG_CRITICAL(condition, logType, msg)                                             \
    if (condition) {                                                                      \
        if (NEO::LogManager::getLoggingLevel() != (uint32_t)NEO::LogLevel::logLevelOff) { \
            auto logger = NEO::LogManager::getInstance()->getLogger(logType);             \
            if (logger) {                                                                 \
                logger->logFatal(msg);                                                    \
            }                                                                             \
        }                                                                                 \
    }

#define LOG_CRITICAL_FOR_CORE(condition, msg) LOG_CRITICAL(condition, NEO::LogManager::LogType::coreLogger, msg)

#define LOG_INFO(logType, msg)                                                        \
    if (NEO::LogManager::getLoggingLevel() != (uint32_t)NEO::LogLevel::logLevelOff) { \
        auto logger = NEO::LogManager::getInstance()->getLogger(logType);             \
        if (logger) {                                                                 \
            logger->logInfo(msg);                                                     \
        }                                                                             \
    }

extern void initLogger();
} // namespace NEO
