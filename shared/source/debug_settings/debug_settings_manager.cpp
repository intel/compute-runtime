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
#include <utility>

namespace NEO {

DebugVariables::DebugVariables() = default;
DebugVariables::DebugVariables(const DebugVariables &other) = default;
DebugVariables::DebugVariables(DebugVariables &&other) = default;
DebugVariables &DebugVariables::operator=(const DebugVariables &other) = default;
DebugVariables &DebugVariables::operator=(DebugVariables &&other) = default;
DebugVariables::~DebugVariables() = default;

#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RAW_ENV_SCOPED_V(dataType, variableName, envVarName, defaultValue, description, ...) \
    DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)

bool DebugVariables::operator==(const DebugVariables &other) const {
#define COMPARE_VARIABLE(variableName)                          \
    if (variableName.getRef() != other.variableName.getRef()) { \
        return false;                                           \
    }
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) COMPARE_VARIABLE(variableName)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) COMPARE_VARIABLE(variableName)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) COMPARE_VARIABLE(variableName)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) COMPARE_VARIABLE(variableName)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description) COMPARE_VARIABLE(variableName)
#define DECLARE_RAW_ENV_VARIABLE_OPT(enabled, dataType, variableName, envVarName, defaultValue, description) COMPARE_VARIABLE(variableName)
#include "env_variables.inl"
#undef DECLARE_RAW_ENV_VARIABLE_OPT
#undef DECLARE_RAW_ENV_VARIABLE
#undef COMPARE_VARIABLE
    return true;
}

template <typename T>
static std::string toString(const T &arg) {
    if constexpr (std::is_convertible_v<std::string, T>) {
        return static_cast<std::string>(arg);
    } else {
        return std::to_string(arg);
    }
}

template <typename DataType>
static void dumpFlagValue(const char *prefix, const char *keyName, const DataType &variableValue, const DataType &defaultValue,
                          std::ostringstream &allFlagsStream, std::ostringstream &changedFlagsStream, bool isEnvOnly) {
    std::string neoKey = prefix;
    neoKey += keyName;
    allFlagsStream << neoKey.c_str() << " = " << variableValue << '\n';
    if (variableValue != defaultValue) {
        const auto variableStringValue = toString(variableValue);
        changedFlagsStream << "Non-default value of debug variable: " << neoKey.c_str() << " = " << variableStringValue.c_str();
        if (isEnvOnly) {
            changedFlagsStream << " (env-only variable)";
        }
        changedFlagsStream << '\n';
    }
}

template <typename DataType>
static void injectDebugSetting(SettingsReader &reader, DVarsScopeMask scope, const char *keyName, DebugVarBase<DataType> &variable) {
    DebugVarPrefix type;
    DataType tempData = reader.getSetting(keyName, variable.get(), type);
    if (0 != (scope & variable.getScopeMask())) {
        variable.setPrefixType(type);
        variable.set(std::move(tempData));
    }
}

template <typename DataType>
static void injectReleaseSetting(SettingsReader &reader, SettingsReader &envOnlyReader, DVarsScopeMask scope, const char *keyName, DebugVarBase<DataType> &variable) {
    DebugVarPrefix type = DebugVarPrefix::none;
    DataType tempData = variable.get();
    if (reader.hasSetting(keyName, type)) {
        tempData = reader.getSetting(keyName, tempData, type);
    } else if (envOnlyReader.hasSetting(keyName, type)) {
        tempData = envOnlyReader.getSetting(keyName, tempData, type);
    }
    if (0 != (scope & variable.getScopeMask())) {
        variable.setPrefixType(type);
        variable.set(std::move(tempData));
    }
}

template <typename DataType>
static void injectEnvSetting(SettingsReader &envOnlyReader, DVarsScopeMask scope, const char *envVarName, DebugVarBase<DataType> &variable) {
    if (0 != (scope & variable.getScopeMask())) {
        variable.set(envOnlyReader.getSetting(envVarName, variable.get()));
    }
}

template <typename DataType>
static void injectBareNameReleaseSetting(SettingsReader &reader, SettingsReader &envOnlyReader, DVarsScopeMask scope, const char *keyName, DebugVarBase<DataType> &variable) {
    if (0 == (scope & variable.getScopeMask())) {
        return;
    }
    DataType tempData = envOnlyReader.getSetting(keyName, variable.get());
    tempData = reader.getSetting(keyName, tempData);
    variable.set(std::move(tempData));
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
void DebugSettingsManager<debugLevel>::getStringWithFlags(std::string &allFlags, std::string &changedFlags) const {
    std::ostringstream allFlagsStream;
    allFlagsStream.str("");

    std::ostringstream changedFlagsStream;
    changedFlagsStream.str("");

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)                                        \
    dumpFlagValue<dataType>(convPrefixToString(flags.variableName.getPrefixType()), getNonReleaseKeyName(#variableName), \
                            flags.variableName.get(), defaultValue, allFlagsStream, changedFlagsStream, false);
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
    dumpFlagValue<dataType>(convPrefixToString(flags.variableName.getPrefixType()), #variableName, \
                            flags.variableName.get(), defaultValue, allFlagsStream, changedFlagsStream, false);
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                     \
        DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)              \
    }
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description) \
    dumpFlagValue<dataType>("", envVarName, flags.variableName.get(), defaultValue, allFlagsStream, changedFlagsStream, true);
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
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    injectDebugSetting(*readerImpl, this->scope, getNonReleaseKeyName(#variableName), flags.variableName);
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
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) \
    injectReleaseSetting(*readerImpl, envOnlyReader, this->scope, #variableName, flags.variableName);
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    if constexpr (enabled) {                                                                     \
        DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)              \
    }
    // A reader that always goes straight to the OS environment, bypassing readerImpl - used both as
    // the release-variable env fallback above and for env variables below.
    EnvironmentVariableReader envOnlyReader;
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description) \
    injectEnvSetting(envOnlyReader, this->scope, envVarName, flags.variableName);
#define DECLARE_RAW_ENV_VARIABLE_OPT(enabled, dataType, variableName, envVarName, defaultValue, description) \
    if constexpr (enabled) {                                                                                 \
        DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)              \
    }
#include "env_variables.inl"
#undef DECLARE_RAW_ENV_VARIABLE_OPT
#undef DECLARE_RAW_ENV_VARIABLE
}

template <DebugFunctionalityLevel debugLevel>
void DebugSettingsManager<debugLevel>::refreshEnvVariables() {
    EnvironmentVariableReader envOnlyReader;
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZE_FLAT_DEVICE_HIERARCHY", flags.ZE_FLAT_DEVICE_HIERARCHY);
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZE_AFFINITY_MASK", flags.ZE_AFFINITY_MASK);
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZEX_NUMBER_OF_CCS", flags.ZEX_NUMBER_OF_CCS);
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZE_ENABLE_PCI_ID_DEVICE_ORDER", flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER);
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZET_ENABLE_PROGRAM_DEBUGGING", flags.ZET_ENABLE_PROGRAM_DEBUGGING);
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZET_ENABLE_METRICS", flags.ZET_ENABLE_METRICS);
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZET_ENABLE_PROGRAM_INSTRUMENTATION", flags.ZET_ENABLE_PROGRAM_INSTRUMENTATION);
    injectBareNameReleaseSetting(*readerImpl, envOnlyReader, this->scope, "ZES_ENABLE_SYSMAN", flags.ZES_ENABLE_SYSMAN);
}

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
