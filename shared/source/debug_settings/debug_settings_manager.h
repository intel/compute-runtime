/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/io_functions.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string_view>

enum class DebugFunctionalityLevel {
    none,   // Debug functionality disabled
    full,   // Debug functionality fully enabled
    regKeys // Only registry key reads enabled
};

#if defined(_DEBUG)
constexpr DebugFunctionalityLevel globalDebugFunctionalityLevel = DebugFunctionalityLevel::full;
#elif defined(_RELEASE_INTERNAL) || defined(_RELEASE_BUILD_WITH_REGKEYS)
constexpr DebugFunctionalityLevel globalDebugFunctionalityLevel = DebugFunctionalityLevel::regKeys;
#else
constexpr DebugFunctionalityLevel globalDebugFunctionalityLevel = DebugFunctionalityLevel::none;
#endif

#define PRINT_DEBUG_STRING(flag, ...) \
    if (flag)                         \
        NEO::printDebugString(flag, __VA_ARGS__);

namespace NEO {
template <DebugFunctionalityLevel debugLevel>
class FileLogger;
extern FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance();

template <typename StreamT, typename... Args>
void flushDebugStream(StreamT stream, Args &&...args) {
    IoFunctions::fflushPtr(stream);
}

template <typename... Args>
void printDebugString(bool showDebugLogs, Args... args) {
    if (showDebugLogs) {
        IoFunctions::fprintf(args...);
        flushDebugStream(args...);
    }
}

void logDebugString(std::string_view debugString);

#if defined(__clang__)
#define NO_SANITIZE __attribute__((no_sanitize("undefined")))
#else
#define NO_SANITIZE
#endif

class SettingsReader;

enum class DebugVarPrefix : uint8_t {
    none = 1,
    neo = 2,
    neoL0 = 3,
    neoOcl = 4
};

template <typename T>
struct DebugVarBase {
    DebugVarBase(const T &defaultValue) : value(defaultValue), defaultValue(defaultValue) {}
    T get() const {
        return value;
    }
    void set(T data) {
        value = std::move(data);
    }
    T &getRef() {
        return value;
    }
    void setIfDefault(T data) {
        if (value == defaultValue) {
            this->set(data);
        }
    }
    void setPrefixType(DebugVarPrefix data) {
        prefixType = std::move(data);
    }
    DebugVarPrefix getPrefixType() const {
        return prefixType;
    }

  private:
    T value;
    T defaultValue;
    DebugVarPrefix prefixType = DebugVarPrefix::none;
};

struct DebugVariables {                                 // NOLINT(clang-analyzer-optin.performance.Padding)
    struct DEBUGGER_LOG_BITMASK {                       // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_INFO{1};           // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_ERROR{1 << 1};     // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_THREADS{1 << 2};   // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_MEM{1 << 3};       // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_FIFO{1 << 4};      // NOLINT(readability-identifier-naming)
        constexpr static int32_t DUMP_ELF{1 << 10};     // NOLINT(readability-identifier-naming)
        constexpr static int32_t DUMP_TO_FILE{1 << 16}; // NOLINT(readability-identifier-naming)
    };

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    DebugVarBase<dataType> variableName{defaultValue};
#include "debug_variables.inl"
#include "release_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
};

template <DebugFunctionalityLevel debugLevel>
class DebugSettingsManager {
  public:
    DebugSettingsManager(const char *registryPath);
    ~DebugSettingsManager();

    DebugSettingsManager(const DebugSettingsManager &) = delete;
    DebugSettingsManager &operator=(const DebugSettingsManager &) = delete;

    static constexpr bool registryReadAvailable() {
        return (debugLevel == DebugFunctionalityLevel::full) || (debugLevel == DebugFunctionalityLevel::regKeys);
    }

    static constexpr bool disabled() {
        return debugLevel == DebugFunctionalityLevel::none;
    }

    void getHardwareInfoOverride(std::string &hwInfoConfig);

    void injectSettingsFromReader();

    DebugVariables flags;
    void *injectFcn = nullptr;

    void setReaderImpl(SettingsReader *newReaderImpl) {
        readerImpl.reset(newReaderImpl);
    }
    SettingsReader *getReaderImpl() {
        return readerImpl.get();
    }

    static constexpr const char *getNonReleaseKeyName(const char *key) {
        return (disabled() && PURGE_DEBUG_KEY_NAMES) ? "" : key;
    }

    void getStringWithFlags(std::string &allFlags, std::string &changedFlags) const;

    template <typename FT>
    void logLazyEvaluateArgs(FT &&callable) {
        if (!disabled()) {
            callable();
        }
    }

  protected:
    std::unique_ptr<SettingsReader> readerImpl;
    bool isLoopAtDriverInitEnabled() const {
        auto loopingEnabled = flags.LoopAtDriverInit.get();
        return loopingEnabled;
    }
    template <typename DataType>
    static void dumpNonDefaultFlag(const char *variableName, const DataType &variableValue, const DataType &defaultValuep, std::ostringstream &ostring);

    void dumpFlags() const;
    static const char *settingsDumpFileName;
};

extern DebugSettingsManager<globalDebugFunctionalityLevel> debugManager;

class DurationLog {
    DurationLog() = delete;

  public:
    static std::string getTimeString();
};

#define PRINT_DEBUGGER_LOG_TO_FILE(...)                            \
    NEO::debugManager.logLazyEvaluateArgs([&] {                    \
        char temp[4000];                                           \
        snprintf_s(temp, sizeof(temp), sizeof(temp), __VA_ARGS__); \
        temp[sizeof(temp) - 1] = '\0';                             \
        NEO::logDebugString(temp);                                 \
    });

#define PRINT_DEBUGGER_LOG(OUT, ...)                                                                                  \
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_TO_FILE) { \
        PRINT_DEBUGGER_LOG_TO_FILE(__VA_ARGS__)                                                                       \
    } else {                                                                                                          \
        NEO::printDebugString(true, OUT, __VA_ARGS__);                                                                \
    }

#define PRINT_DEBUGGER_INFO_LOG(STR, ...)                                                                         \
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO) { \
                                                                                                                  \
        auto time = NEO::DurationLog::getTimeString();                                                            \
        time = "\n" + time + " INFO: " + STR;                                                                     \
        PRINT_DEBUGGER_LOG(stdout, time.c_str(), __VA_ARGS__)                                                     \
    }

#define PRINT_DEBUGGER_THREAD_LOG(STR, ...)                                                                          \
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_THREADS) { \
                                                                                                                     \
        auto time = NEO::DurationLog::getTimeString();                                                               \
        time = "\n" + time + " THREAD INFO: " + STR;                                                                 \
        PRINT_DEBUGGER_LOG(stdout, time.c_str(), __VA_ARGS__)                                                        \
    }

#define PRINT_DEBUGGER_ERROR_LOG(STR, ...)                                                                         \
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR) { \
                                                                                                                   \
        auto time = NEO::DurationLog::getTimeString();                                                             \
        time = "\n" + time + " ERROR: " + STR;                                                                     \
        PRINT_DEBUGGER_LOG(stderr, time.c_str(), __VA_ARGS__)                                                      \
    }

#define PRINT_DEBUGGER_MEM_ACCESS_LOG(STR, ...)                                                                  \
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_MEM) { \
                                                                                                                 \
        auto time = NEO::DurationLog::getTimeString();                                                           \
        time = "\n" + time + " MEM_ACCESS: " + STR;                                                              \
        PRINT_DEBUGGER_LOG(stdout, time.c_str(), __VA_ARGS__)                                                    \
    }

#define PRINT_DEBUGGER_FIFO_LOG(STR, ...)                                                                         \
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_FIFO) { \
                                                                                                                  \
        auto time = NEO::DurationLog::getTimeString();                                                            \
        time = "\n" + time + " FIFO ACCESS: " + STR;                                                              \
        PRINT_DEBUGGER_LOG(stdout, time.c_str(), __VA_ARGS__)                                                     \
    }

template <DebugFunctionalityLevel debugLevel>
const char *DebugSettingsManager<debugLevel>::settingsDumpFileName = "igdrcl_dumped.config";
}; // namespace NEO
