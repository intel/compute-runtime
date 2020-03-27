/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/debug_env_reader.h"

namespace NEO {

SettingsReader *SettingsReader::createOsReader(bool userScope, const std::string &regKey) {
    return new EnvironmentVariableReader;
}

const char *EnvironmentVariableReader::appSpecificLocation(const std::string &name) {
    return name.c_str();
}

bool EnvironmentVariableReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int64_t>(defaultValue)) ? true : false;
}

int32_t EnvironmentVariableReader::getSetting(const char *settingName, int32_t defaultValue) {
    return static_cast<int32_t>(getSetting(settingName, static_cast<int64_t>(defaultValue)));
}

int64_t EnvironmentVariableReader::getSetting(const char *settingName, int64_t defaultValue) {
    int64_t value = defaultValue;
    char *envValue;

    envValue = getenv(settingName);
    if (envValue) {
        value = atoi(envValue);
    }
    return value;
}

std::string EnvironmentVariableReader::getSetting(const char *settingName, const std::string &value) {
    char *envValue;
    std::string keyValue;
    keyValue.assign(value);

    envValue = getenv(settingName);
    if (envValue) {
        keyValue.assign(envValue);
    }
    return keyValue;
}
} // namespace NEO
