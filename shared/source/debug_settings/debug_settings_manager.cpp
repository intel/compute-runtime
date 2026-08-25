/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "debug_settings_manager.h"

#include "shared/source/debug_settings/debug_variables_helper.h"
#include "shared/source/debug_settings/definitions/translate_debug_settings.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/utilities/debug_settings_reader_creator.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/source/utilities/logger.h"

#include <chrono>
#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <type_traits>

namespace NEO {

#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RAW_ENV_SCOPED_V(dataType, variableName, envVarName, defaultValue, description, ...) \
    DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)

template <typename T>
static std::string toString(const T &arg) {
    if constexpr (std::is_convertible_v<std::string, T>) {
        return static_cast<std::string>(arg);
    } else {
        return std::to_string(arg);
    }
}

template <DebugFunctionalityLevel debugLevel>
DebugSettingsManager<debugLevel>::DebugSettingsManager(const char *registryPath) {
    readerImpl = SettingsReaderCreator::create(std::string(registryPath));
    ApiSpecificConfig::initPrefixes();
    for (auto prefixType : ApiSpecificConfig::getPrefixTypes()) {
        this->scope |= getDebugVarScopeMaskFor(prefixType);
    }
    injectSettingsFromReader();
    dumpFlags();
    translateDebugSettings(flags);

    while (isLoopAtDriverInitEnabled()) {
        ;
    }
}

template <DebugFunctionalityLevel debugLevel>
DebugSettingsManager<debugLevel>::~DebugSettingsManager() {
    readerImpl.reset();
};

template <DebugFunctionalityLevel debugLevel>
void DebugSettingsManager<debugLevel>::getHardwareInfoOverride(std::string &hwInfoConfig) {
    std::string str = flags.HardwareInfoOverride.get();
    if (str[0] == '\"') {
        str.pop_back();
        hwInfoConfig = str.substr(1, std::string::npos);
    } else {
        hwInfoConfig = str;
    }
}

static const char *convPrefixToString(DebugVarPrefix prefix) {
    if (prefix == DebugVarPrefix::neo) {
        return "NEO_";
    } else if (prefix == DebugVarPrefix::neoL0) {
        return "NEO_L0_";
    } else if (prefix == DebugVarPrefix::neoOcl) {
        return "NEO_OCL_";
    } else if (prefix == DebugVarPrefix::neoOcloc) {
        return "NEO_OCLOC_";
    } else {
        return "";
    }
}

template <DebugFunctionalityLevel debugLevel>
template <typename DataType>
void DebugSettingsManager<debugLevel>::dumpNonDefaultFlag(const char *variableName, const DataType &variableValue, const DataType &defaultValue, std::ostringstream &ostring, bool isEnvOnly) {
    if (variableValue != defaultValue) {
        const auto variableStringValue = toString(variableValue);
        ostring << "Non-default value of debug variable: " << variableName << " = " << variableStringValue.c_str();
        if (isEnvOnly) {
            ostring << " (env-only variable)";
        }
        ostring << '\n';
    }
}

template <DebugFunctionalityLevel debugLevel>
void DebugSettingsManager<debugLevel>::getStringWithFlags(std::string &allFlags, std::string &changedFlags) const {
    std::ostringstream allFlagsStream;
    allFlagsStream.str("");

    std::ostringstream changedFlagsStream;
    changedFlagsStream.str("");

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)                                        \
    {                                                                                                                    \
        std::string neoKey = convPrefixToString(flags.variableName.getPrefixType());                                     \
        constexpr auto keyName = getNonReleaseKeyName(#variableName);                                                    \
        neoKey += keyName;                                                                                               \
        allFlagsStream << neoKey.c_str() << " = " << flags.variableName.get() << '\n';                                   \
        dumpNonDefaultFlag<dataType>(neoKey.c_str(), flags.variableName.get(), defaultValue, changedFlagsStream, false); \
    }
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                   \
        DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)              \
    }
#if !defined(NEO_USE_CONSTEXPR_DEBUG_VARIABLES)
    if (registryReadAvailable() || isDebugKeysReadEnabled()) {
#include "debug_variables.inl"
    }
#endif
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)                                      \
    {                                                                                                                    \
        std::string neoKey = convPrefixToString(flags.variableName.getPrefixType());                                     \
        neoKey += #variableName;                                                                                         \
        allFlagsStream << neoKey.c_str() << " = " << flags.variableName.get() << '\n';                                   \
        dumpNonDefaultFlag<dataType>(neoKey.c_str(), flags.variableName.get(), defaultValue, changedFlagsStream, false); \
    }
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                     \
        DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)              \
    }
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description) DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                               \
        DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description)              \
    }
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)                 \
    {                                                                                                           \
        constexpr auto neoKey = envVarName;                                                                     \
        allFlagsStream << neoKey << " = " << flags.variableName.get() << '\n';                                  \
        dumpNonDefaultFlag<dataType>(neoKey, flags.variableName.get(), defaultValue, changedFlagsStream, true); \
    }
#define DECLARE_RAW_ENV_VARIABLE_OPT(enabled, dataType, variableName, envVarName, defaultValue, description) \
    if constexpr (enabled) {                                                                                 \
        DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)              \
    }
#include "env_variables.inl"
#undef DECLARE_RAW_ENV_VARIABLE_OPT
#undef DECLARE_RAW_ENV_VARIABLE

    allFlags = allFlagsStream.str();
    changedFlags = changedFlagsStream.str();
}

template <DebugFunctionalityLevel debugLevel>
void DebugSettingsManager<debugLevel>::dumpFlags() const {
    if (flags.PrintDebugSettings.get() == false) {
        return;
    }

    std::string allFlags;
    std::string changedFlags;

    getStringWithFlags(allFlags, changedFlags);
    PRINT_STRING(true, stdout, "%s", changedFlags.c_str());

    NEO::writeDataToFile(settingsDumpFileName, allFlags, false);
}

template <DebugFunctionalityLevel debugLevel>
void DebugSettingsManager<debugLevel>::injectSettingsFromReader() {
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)            \
    {                                                                                        \
        DebugVarPrefix type;                                                                 \
        constexpr auto keyName = getNonReleaseKeyName(#variableName);                        \
        dataType tempData = readerImpl->getSetting(keyName, flags.variableName.get(), type); \
        if (0 != (this->scope & flags.variableName.getScopeMask())) {                        \
            flags.variableName.setPrefixType(type);                                          \
            flags.variableName.set(tempData);                                                \
        }                                                                                    \
    }
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                   \
        DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)              \
    }

#if !defined(NEO_USE_CONSTEXPR_DEBUG_VARIABLES)
    if (registryReadAvailable() || isDebugKeysReadEnabled()) {
#include "debug_variables.inl"
    }
#endif
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)                \
    {                                                                                              \
        DebugVarPrefix type;                                                                       \
        dataType tempData = readerImpl->getSetting(#variableName, flags.variableName.get(), type); \
        if (0 != (this->scope & flags.variableName.getScopeMask())) {                              \
            flags.variableName.setPrefixType(type);                                                \
            flags.variableName.set(tempData);                                                      \
        }                                                                                          \
    }
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                     \
        DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)              \
    }
    // Env variables are looked up by their exact name directly in the OS environment, regardless of
    // readerImpl - a NEO settings file must never be able to override a plain OS/spec environment variable.
    EnvironmentVariableReader envOnlyReader;
    // "Env-first" release variables take priority over readerImpl (file or registry) whenever the
    // exact literal name is genuinely present in the real OS environment; otherwise they fall back to
    // the normal readerImpl-based release variable read, unchanged.
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description)          \
    if (nullptr != IoFunctions::getenvPtr(#variableName)) {                                            \
        if (0 != (this->scope & flags.variableName.getScopeMask())) {                                  \
            flags.variableName.setPrefixType(DebugVarPrefix::none);                                    \
            flags.variableName.set(envOnlyReader.getSetting(#variableName, flags.variableName.get())); \
        }                                                                                              \
    } else {                                                                                           \
        DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)                    \
    }
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                               \
        DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description)              \
    }

#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description) \
    if (0 != (this->scope & flags.variableName.getScopeMask())) {                               \
        flags.variableName.set(envOnlyReader.getSetting(envVarName, flags.variableName.get())); \
    }
#define DECLARE_RAW_ENV_VARIABLE_OPT(enabled, dataType, variableName, envVarName, defaultValue, description) \
    if constexpr (enabled) {                                                                                 \
        DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)              \
    }
#include "env_variables.inl"
#undef DECLARE_RAW_ENV_VARIABLE_OPT
#undef DECLARE_RAW_ENV_VARIABLE
} // namespace NEO

void logDebugString(std::string_view debugString) {
    NEO::fileLoggerInstance().logDebugString(true, debugString);
}

std::string DurationLog::getTimeString() {
    auto now = std::chrono::steady_clock::now();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    auto seconds = microseconds.count() / 1000000;
    auto remainingMicroSeconds = microseconds.count() % 1000000;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "[%5" PRId64 ".%06" PRId64 "]",
                  static_cast<int64_t>(seconds), static_cast<int64_t>(remainingMicroSeconds));
    return std::string(buffer);
}

std::string DurationLog::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    tm timeInfo = *std::localtime(&nowTime);

    std::stringstream ss;

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", &timeInfo);
    ss << buffer;

    return ss.str();
}

template class DebugSettingsManager<DebugFunctionalityLevel::none>;
template class DebugSettingsManager<DebugFunctionalityLevel::full>;
template class DebugSettingsManager<DebugFunctionalityLevel::regKeys>;
}; // namespace NEO
