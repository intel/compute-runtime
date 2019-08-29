/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/debug_env_reader.h"

namespace NEO {

SettingsReader *SettingsReader::createOsReader(bool userScope) {
    return new EnvironmentVariableReader;
}

SettingsReader *SettingsReader::createOsReader(const std::string &regKey) {
    return new EnvironmentVariableReader;
}
const char *EnvironmentVariableReader::appSpecificLocation(const std::string &name) {
    return name.c_str();
}

bool EnvironmentVariableReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int32_t>(defaultValue)) ? true : false;
}

int32_t EnvironmentVariableReader::getSetting(const char *settingName, int32_t defaultValue) {
    int32_t value = defaultValue;
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
