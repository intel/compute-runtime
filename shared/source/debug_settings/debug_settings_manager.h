/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

enum class DebugFunctionalityLevel {
    None,   // Debug functionality disabled
    Full,   // Debug functionality fully enabled
    RegKeys // Only registry key reads enabled
};

#if defined(_DEBUG)
constexpr DebugFunctionalityLevel globalDebugFunctionalityLevel = DebugFunctionalityLevel::Full;
#elif defined(_RELEASE_INTERNAL) || defined(_RELEASE_BUILD_WITH_REGKEYS)
constexpr DebugFunctionalityLevel globalDebugFunctionalityLevel = DebugFunctionalityLevel::RegKeys;
#else
constexpr DebugFunctionalityLevel globalDebugFunctionalityLevel = DebugFunctionalityLevel::None;
#endif

#define PRINT_DEBUG_STRING(flag, ...) \
    if (flag)                         \
        NEO::printDebugString(flag, __VA_ARGS__);

namespace NEO {

template <typename StreamT, typename... Args>
void flushDebugStream(StreamT stream, Args &&...args) {
    fflush(stream);
}

template <typename... Args>
void printDebugString(bool showDebugLogs, Args &&...args) {
    if (showDebugLogs) {
        fprintf(std::forward<Args>(args)...);
        flushDebugStream(args...);
    }
}

#if defined(__clang__)
#define NO_SANITIZE __attribute__((no_sanitize("undefined")))
#else
#define NO_SANITIZE
#endif

class SettingsReader;

template <typename T>
struct DebugVarBase {
    DebugVarBase(const T &defaultValue) : value(defaultValue) {}
    T get() const {
        return value;
    }
    void set(T data) {
        value = std::move(data);
    }
    T &getRef() {
        return value;
    }

  private:
    T value;
};

struct DebugVariables { // NOLINT(clang-analyzer-optin.performance.Padding)
    struct DEBUGGER_LOG_BITMASK {
        constexpr static int32_t LOG_INFO{1};         // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_ERROR{1 << 1};   // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_THREADS{1 << 2}; // NOLINT(readability-identifier-naming)
        constexpr static int32_t LOG_MEM{1 << 3};     // NOLINT(readability-identifier-naming)
        constexpr static int32_t DUMP_ELF{1 << 10};   // NOLINT(readability-identifier-naming)
    };

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    DebugVarBase<dataType> variableName{defaultValue};
#include "debug_variables.inl"
#include "release_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
};

template <DebugFunctionalityLevel DebugLevel>
class DebugSettingsManager {
  public:
    DebugSettingsManager(const char *registryPath);
    ~DebugSettingsManager();

    DebugSettingsManager(const DebugSettingsManager &) = delete;
    DebugSettingsManager &operator=(const DebugSettingsManager &) = delete;

    static constexpr bool registryReadAvailable() {
        return (DebugLevel == DebugFunctionalityLevel::Full) || (DebugLevel == DebugFunctionalityLevel::RegKeys);
    }

    static constexpr bool disabled() {
        return DebugLevel == DebugFunctionalityLevel::None;
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

  protected:
    std::unique_ptr<SettingsReader> readerImpl;

    bool isLoopAtDriverInitEnabled() const {
        auto loopingEnabled = flags.LoopAtDriverInit.get();
        return loopingEnabled;
    }
    template <typename DataType>
    static void dumpNonDefaultFlag(const char *variableName, const DataType &variableValue, const DataType &defaultValue);
    void dumpFlags() const;
    static const char *settingsDumpFileName;
};

extern DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager;

#define PRINT_DEBUGGER_LOG(OUT, STR, ...) \
    NEO::printDebugString(true, OUT, STR, __VA_ARGS__);

#define PRINT_DEBUGGER_INFO_LOG(STR, ...)                                                                         \
    if (NEO::DebugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO) { \
        PRINT_DEBUGGER_LOG(stdout, "\nINFO: " STR, __VA_ARGS__)                                                   \
    }

#define PRINT_DEBUGGER_THREAD_LOG(STR, ...)                                                                          \
    if (NEO::DebugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_THREADS) { \
        PRINT_DEBUGGER_LOG(stdout, "\nTHREAD INFO: " STR, __VA_ARGS__)                                               \
    }

#define PRINT_DEBUGGER_ERROR_LOG(STR, ...)                                                                         \
    if (NEO::DebugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR) { \
        PRINT_DEBUGGER_LOG(stderr, "\nERROR: " STR, __VA_ARGS__)                                                   \
    }

#define PRINT_DEBUGGER_MEM_ACCESS_LOG(STR, ...)                                                                  \
    if (NEO::DebugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_MEM) { \
        PRINT_DEBUGGER_LOG(stdout, "\nINFO: " STR, __VA_ARGS__)                                                  \
    }

template <DebugFunctionalityLevel DebugLevel>
const char *DebugSettingsManager<DebugLevel>::settingsDumpFileName = "igdrcl_dumped.config";
}; // namespace NEO
