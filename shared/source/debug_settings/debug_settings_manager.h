/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/options.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/timestamp.h"
#include "shared/source/utilities/io_functions.h"

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#if defined(_WIN32)
#include <process.h>
#pragma warning(disable : 4996)
#else
#include <unistd.h>
#endif

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

#define PRINT_DEBUG_STRING(flag, stream, ...) \
    if (flag)                                 \
        NEO::printDebugString(flag, stream, __VA_ARGS__);

#define EMIT_WARNING(flag, stream, ...) PRINT_DEBUG_STRING(flag, stream, __VA_ARGS__)

namespace NEO {
template <DebugFunctionalityLevel debugLevel>
class FileLogger;
extern FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance();

template <typename StreamT, typename... Args>
void flushDebugStream(StreamT stream, Args &&...args) {
    IoFunctions::fflushPtr(stream);
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
    neoOcl = 4,
    neoOcloc = 5
};

using DVarsScopeMask = std::underlying_type_t<DebugVarPrefix>;
constexpr auto getDebugVarScopeMaskFor(DebugVarPrefix v) {
    return static_cast<DVarsScopeMask>(1U) << static_cast<DVarsScopeMask>(v);
}

template <DebugVarPrefix... vs>
constexpr auto getDebugVarScopeMaskFor() {
    return (0 | ... | getDebugVarScopeMaskFor(vs));
}

// compatibility with "old" behavior (prior to introducing scope masks)
constexpr inline DVarsScopeMask compatibilityMask = getDebugVarScopeMaskFor<DebugVarPrefix::neoL0, DebugVarPrefix::neoOcl>();

template <typename T>
struct DebugVarBase {
    DebugVarBase(const T &defaultValue) : value(defaultValue), defaultValue(defaultValue) {}
    DebugVarBase(const T &defaultValue, DVarsScopeMask scopeMask) : value(defaultValue), defaultValue(defaultValue), scopeMask(scopeMask) {}
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
    DVarsScopeMask getScopeMask() const {
        return scopeMask;
    }

  private:
    T value;
    T defaultValue;
    DebugVarPrefix prefixType = DebugVarPrefix::none;
    DVarsScopeMask scopeMask = compatibilityMask;
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
#define S_NONE getDebugVarScopeMaskFor(DebugVarPrefix::none)
#define S_NEO getDebugVarScopeMaskFor(DebugVarPrefix::neo)
#define S_OCL getDebugVarScopeMaskFor(DebugVarPrefix::neoOcl)
#define S_L0 getDebugVarScopeMaskFor(DebugVarPrefix::neoL0)
#define S_RT (S_NEO | S_OCL | S_L0 | S_NONE)
#define S_OCLOC getDebugVarScopeMaskFor(DebugVarPrefix::neoOcloc)
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, scope, description) \
    DebugVarBase<dataType> variableName{defaultValue, scope};
#include "debug_variables.inl"
#include "release_variables.inl"
#undef S_OCLOC
#undef S_RT
#undef S_L0
#undef S_OCL
#undef S_NEO
#undef S_NONE
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
};

template <DebugFunctionalityLevel debugLevel>
class DebugSettingsManager : NEO::NonCopyableAndNonMovableClass {
  public:
    DebugSettingsManager(const char *registryPath);
    ~DebugSettingsManager();

    static consteval bool registryReadAvailable() {
        return (debugLevel == DebugFunctionalityLevel::full) || (debugLevel == DebugFunctionalityLevel::regKeys);
    }

    static consteval bool disabled() {
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

    static consteval const char *getNonReleaseKeyName(const char *key) {
        return (disabled() && PURGE_DEBUG_KEY_NAMES) ? "" : key;
    }

    void getStringWithFlags(std::string &allFlags, std::string &changedFlags) const;

    template <typename FT>
    void logLazyEvaluateArgs(FT &&callable) {
        if (!disabled()) {
            callable();
        }
    }

    inline bool isTbxPageFaultManagerEnabled() {
        auto setCsr = flags.SetCommandStreamReceiver.get();
        auto isTbxMode = (setCsr == static_cast<int32_t>(CommandStreamReceiverType::tbx)) ||
                         (setCsr == static_cast<int32_t>(CommandStreamReceiverType::tbxWithAub));
        auto isFaultManagerEnabledInEnvVars = true;
        if (flags.EnableTbxPageFaultManager.get() == 0) {
            isFaultManagerEnabledInEnvVars = false;
        }
        return isFaultManagerEnabledInEnvVars && isTbxMode;
    }

  protected:
    DVarsScopeMask scope = 0;
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

static_assert(NEO::NonCopyableAndNonMovable<DebugSettingsManager<DebugFunctionalityLevel::none>>);
static_assert(NEO::NonCopyableAndNonMovable<DebugSettingsManager<DebugFunctionalityLevel::full>>);
static_assert(NEO::NonCopyableAndNonMovable<DebugSettingsManager<DebugFunctionalityLevel::regKeys>>);

extern DebugSettingsManager<globalDebugFunctionalityLevel> debugManager;

struct DebugMessagesBitmask {
    constexpr static int32_t withPid = 1 << 0;
    constexpr static int32_t withTimestamp = 1 << 1;
};

template <typename... Args>
void printDebugString(bool showDebugLogs, FILE *stream, Args... args) {
    if (showDebugLogs) {
        if (NEO::debugManager.flags.DebugMessagesBitmask.get() & DebugMessagesBitmask::withPid) {
            IoFunctions::fprintf(stream, "[PID: %d] ", getpid());
        }
        if (NEO::debugManager.flags.DebugMessagesBitmask.get() & DebugMessagesBitmask::withTimestamp) {
            IoFunctions::fprintf(stream, "%s", TimestampHelper::getTimestamp().c_str());
        }
        IoFunctions::fprintf(stream, args...);
        flushDebugStream(stream, args...);
    }
}

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
