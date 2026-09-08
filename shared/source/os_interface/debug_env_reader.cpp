/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/debug_env_reader.h"

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/io_functions.h"

#include <vector>

namespace NEO {

const char *EnvironmentVariableReader::appSpecificLocation(const std::string &name) {
    return name.c_str();
}

char *EnvironmentVariableReader::getEnvironmentVariable(const char *name) {
    char *environmentVariable = IoFunctions::getenvPtr(name);

    if (strnlen_s(environmentVariable, CommonConstants::maxAllowedEnvVariableSize) < CommonConstants::maxAllowedEnvVariableSize) {
        return environmentVariable;
    }

    return nullptr;
}

char *EnvironmentVariableReader::findEnvironmentVariable(const char *settingName, DebugVarPrefix &type) {
    auto prefixString = ApiSpecificConfig::getPrefixStrings();
    auto prefixType = ApiSpecificConfig::getPrefixTypes();

    uint32_t i = 0;
    for (const auto &prefix : prefixString) {
        std::string neoKey = prefix;
        neoKey += settingName;
        if (auto envValue = getEnvironmentVariable(neoKey.c_str())) {
            type = prefixType[i];
            return envValue;
        }
        i++;
    }
    type = DebugVarPrefix::none;
    return nullptr;
}

bool EnvironmentVariableReader::hasSetting(const char *settingName, DebugVarPrefix &type) {
    return nullptr != findEnvironmentVariable(settingName, type);
}

bool EnvironmentVariableReader::getSetting(const char *settingName, bool defaultValue, DebugVarPrefix &type) {
    return getSetting(settingName, static_cast<int64_t>(defaultValue), type) ? true : false;
}

bool EnvironmentVariableReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int64_t>(defaultValue)) ? true : false;
}

int32_t EnvironmentVariableReader::getSetting(const char *settingName, int32_t defaultValue, DebugVarPrefix &type) {
    return static_cast<int32_t>(getSetting(settingName, static_cast<int64_t>(defaultValue), type));
}

int32_t EnvironmentVariableReader::getSetting(const char *settingName, int32_t defaultValue) {
    return static_cast<int32_t>(getSetting(settingName, static_cast<int64_t>(defaultValue)));
}

int64_t EnvironmentVariableReader::getSetting(const char *settingName, int64_t defaultValue, DebugVarPrefix &type) {
    if (auto envValue = findEnvironmentVariable(settingName, type)) {
        return atoll(envValue);
    }
    return defaultValue;
}

int64_t EnvironmentVariableReader::getSetting(const char *settingName, int64_t defaultValue) {
    int64_t value = defaultValue;

    if (auto envValue = getEnvironmentVariable(settingName)) {
        value = atoll(envValue);
    }
    return value;
}

std::string EnvironmentVariableReader::getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) {
    if (auto envValue = findEnvironmentVariable(settingName, type)) {
        return std::string(envValue);
    }
    return value;
}

std::string EnvironmentVariableReader::getSetting(const char *settingName, const std::string &value) {
    std::string keyValue = value;
    char *envValue = getEnvironmentVariable(settingName);

    if (envValue) {
        keyValue.assign(envValue);
    }

    return keyValue;
}

} // namespace NEO
