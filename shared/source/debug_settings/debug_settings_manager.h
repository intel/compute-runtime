/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdint.h>
#include <string>
#include <thread>

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

namespace NEO {
template <typename... Args>
void printDebugString(bool showDebugLogs, Args &&... args) {
    if (showDebugLogs) {
        fprintf(std::forward<Args>(args)...);
    }
}
#if defined(__clang__)
#define NO_SANITIZE __attribute__((no_sanitize("undefined")))
#else
#define NO_SANITIZE
#endif

class Kernel;
class GraphicsAllocation;
struct MultiDispatchInfo;
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

struct DebugVariables {
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    DebugVarBase<dataType> variableName{defaultValue};
#include "debug_variables.inl"
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

  protected:
    std::unique_ptr<SettingsReader> readerImpl;
    std::mutex mtx;
    std::string logFileName;

    template <typename DataType>
    static void dumpNonDefaultFlag(const char *variableName, const DataType &variableValue, const DataType &defaultValue);
    void dumpFlags() const;
    static const char *settingsDumpFileName;
};

extern DebugSettingsManager<globalDebugFunctionalityLevel> DebugManager;

template <DebugFunctionalityLevel DebugLevel>
const char *DebugSettingsManager<DebugLevel>::settingsDumpFileName = "igdrcl_dumped.config";
}; // namespace NEO
