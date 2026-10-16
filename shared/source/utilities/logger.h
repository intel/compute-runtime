/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <ios>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#define CREATE_DEBUG_STRING(buffer, format, ...)                          \
    auto buffer = std::make_unique<char[]>(NEO::maxErrorDescriptionSize); \
    snprintf(buffer.get(), NEO::maxErrorDescriptionSize, format, __VA_ARGS__)

namespace NEO {

static const int32_t maxErrorDescriptionSize = 1024;
std::string getFileLoggerFileName(const DebugVariables &flags);

template <DebugFunctionalityLevel debugLevel>
class FileLogger : NEO::NonCopyableAndNonMovableClass {
  public:
    FileLogger(std::string filename);
    MOCKABLE_VIRTUAL ~FileLogger() = default;

    static constexpr bool enabled() {
        return debugLevel != DebugFunctionalityLevel::none;
    }

    MOCKABLE_VIRTUAL void writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode);

    template <typename FT>
    void logLazyEvaluateArgs(bool predicate, FT &&callable) {
        if (enabled()) {
            if (predicate) {
                callable();
            }
        }
    }

    template <typename... Types>
    void log(bool enableLog, const Types &...params) {
        if (enabled()) {
            if (enableLog) {
                std::thread::id thisThread = std::this_thread::get_id();
                std::stringstream ss;
                this->print(ss, "ThreadID", thisThread, params...);

                const auto str = ss.str();
                this->writeToFile(this->logFileName, str.c_str(), str.length(), std::ios::app);
            }
        }
    }

    void logDebugString(bool enableLog, std::string_view debugString);

    const char *getLogFileName() { return this->logFileName.c_str(); }

    void setLogFileName(std::string filename) { this->logFileName = std::move(filename); }

  protected:
    std::mutex mutex;
    std::string logFileName;

    // Required for variadic template with 0 args passed
    void print(std::stringstream &ss) {}

    template <typename T1, typename... Types>
    void print(std::stringstream &ss, const T1 &first, const Types &...params) {
        const size_t argsLeft = sizeof...(params);

        ss << first << " ";
        if (argsLeft == 0) {
            ss << std::endl;
        }
        this->print(ss, params...);
    }
};

static_assert(NEO::NonCopyableAndNonMovable<FileLogger<DebugFunctionalityLevel::none>>);
static_assert(NEO::NonCopyableAndNonMovable<FileLogger<DebugFunctionalityLevel::full>>);
static_assert(NEO::NonCopyableAndNonMovable<FileLogger<DebugFunctionalityLevel::regKeys>>);

class UsmReusePerfLogger final : public FileLogger<globalDebugFunctionalityLevel> {
  public:
    UsmReusePerfLogger();
};
static_assert(NEO::NonCopyableAndNonMovable<UsmReusePerfLogger>);

extern FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance();

}; // namespace NEO

#define DBG_LOG_LAZY_EVALUATE_ARGS(LOGGER, PREDICATE, LOG_FUNCTION, ...) \
    LOGGER.logLazyEvaluateArgs(PREDICATE, [&] { LOGGER.LOG_FUNCTION(__VA_ARGS__); })

#define DBG_LOG(PREDICATE, ...) \
    DBG_LOG_LAZY_EVALUATE_ARGS(NEO::fileLoggerInstance(), NEO::debugManager.flags.PREDICATE.get(), log, NEO::debugManager.flags.PREDICATE.get(), __VA_ARGS__)
