/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "linux_logging.h"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

namespace NEO {

LinuxLogger::~LinuxLogger() {
    if (!loggerObj) {
        return;
    }
    loggerObj->flush();
}

int32_t LinuxLogger::createLogger(std::string loggerName, std::string filename, uint32_t logLevel) {
    char *homeDir = getenv("HOME");
    if (homeDir) {
        logFolderPath = std::string(homeDir) + std::string("/.oneapi_logs/");
    }
    int ret = createSpdLogger(loggerName, logFolderPath + filename);
    if (ret == logResultOk) {
        setLevel(logLevel);
    }
    return ret;
}

void LinuxLogger::setLevel(uint32_t logLevel) {
    if (!loggerObj)
        return;
    setSpdLevel(logLevel);
}

void LinuxLogger::logTrace(const char *msg) {
    if (!loggerObj)
        return;
    logSpdTrace(msg);
}

void LinuxLogger::logDebug(const char *msg) {
    if (!loggerObj)
        return;
    logSpdDebug(msg);
}

void LinuxLogger::logInfo(const char *msg) {
    if (!loggerObj)
        return;
    logSpdInfo(msg);
}

void LinuxLogger::logWarning(const char *msg) {
    if (!loggerObj)
        return;
    logSpdWarning(msg);
}

void LinuxLogger::logError(const char *msg) {
    if (!loggerObj)
        return;
    logSpdError(msg);
}

void LinuxLogger::logFatal(const char *msg) {
    if (!loggerObj)
        return;
    logSpdFatal(msg);
}

// spd wrappers
void LinuxLogger::setSpdLevel(uint32_t logLevel) {
    // validate and set log level
    if (logLevel == (uint32_t)NEO::LogLevel::logLevelTrace) {
        loggerObj->set_level(spdlog::level::trace);
    } else if (logLevel == (uint32_t)NEO::LogLevel::logLevelDebug) {
        loggerObj->set_level(spdlog::level::debug);
    } else if (logLevel == (uint32_t)LogLevel::logLevelInfo) {
        loggerObj->set_level(spdlog::level::info);
    } else if (logLevel == (uint32_t)LogLevel::logLevelWarn) {
        loggerObj->set_level(spdlog::level::warn);
    } else if (logLevel == (uint32_t)LogLevel::logLevelError) {
        loggerObj->set_level(spdlog::level::err);
    } else if (logLevel == (uint32_t)LogLevel::logLevelFatal) {
        loggerObj->set_level(spdlog::level::critical);
    } else if (logLevel == (uint32_t)LogLevel::logLevelOff) {
        loggerObj->set_level(spdlog::level::off);
    } else {
        logWarning("Invalid logging level set!!");
    }
    spdlog::flush_on(spdlog::level::trace);
}

void LinuxLogger::logSpdTrace(const char *msg) {
    loggerObj->trace(msg);
}
void LinuxLogger::logSpdDebug(const char *msg) {
    loggerObj->debug(msg);
}
void LinuxLogger::logSpdInfo(const char *msg) {
    loggerObj->info(msg);
}
void LinuxLogger::logSpdWarning(const char *msg) {
    loggerObj->warn(msg);
}
void LinuxLogger::logSpdError(const char *msg) {
    loggerObj->error(msg);
}
void LinuxLogger::logSpdFatal(const char *msg) {
    loggerObj->critical(msg);
}

int32_t LinuxLogger::createSpdLogger(std::string loggerName, std::string filepath) {
    try {
        loggerObj = spdlog::basic_logger_st(loggerName, filepath);
    } catch (spdlog::spdlog_ex &exception) {
        std::cerr << "Unable to create log file: " << exception.what() << "\n";
        return logResultError;
    }
    return logResultOk;
}

} // namespace NEO
