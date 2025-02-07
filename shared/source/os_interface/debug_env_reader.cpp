/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/debug_env_reader.h"

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/utilities/io_functions.h"

#include <vector>

namespace NEO {

const char *EnvironmentVariableReader::appSpecificLocation(const std::string &name) {
    return name.c_str();
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
    int64_t value = defaultValue;
    char *envValue;

    auto prefixString = ApiSpecificConfig::getPrefixStrings();
    auto prefixType = ApiSpecificConfig::getPrefixTypes();
    uint32_t i = 0;

    for (const auto &prefix : prefixString) {
        std::string neoKey = prefix;
        neoKey += settingName;
        envValue = IoFunctions::getenvPtr(neoKey.c_str());
        if (envValue) {
            value = atoll(envValue);
            type = prefixType[i];
            return value;
        }
        i++;
    }
    type = DebugVarPrefix::none;
    return value;
}

int64_t EnvironmentVariableReader::getSetting(const char *settingName, int64_t defaultValue) {
    int64_t value = defaultValue;
    char *envValue;

    envValue = IoFunctions::getenvPtr(settingName);
    if (envValue) {
        value = atoll(envValue);
    }
    return value;
}

std::string EnvironmentVariableReader::getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) {
    std::string keyValue = value;

    auto prefixString = ApiSpecificConfig::getPrefixStrings();
    auto prefixType = ApiSpecificConfig::getPrefixTypes();

    uint32_t i = 0;
    for (const auto &prefix : prefixString) {
        std::string neoKey = prefix;
        neoKey += settingName;
        auto envValue = IoFunctions::getEnvironmentVariable(neoKey.c_str());

        if (envValue) {
            keyValue.assign(envValue);
            type = prefixType[i];
            return keyValue;
        }
        i++;
    }
    type = DebugVarPrefix::none;
    return keyValue;
}

std::string EnvironmentVariableReader::getSetting(const char *settingName, const std::string &value) {
    std::string keyValue = value;
    char *envValue = IoFunctions::getEnvironmentVariable(settingName);

    if (envValue) {
        keyValue.assign(envValue);
    }

    return keyValue;
}

} // namespace NEO
