/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/linux/debug_env_reader.h"

namespace OCLRT {

SettingsReader *SettingsReader::createOsReader(bool userScope) {
    return new EnvironmentVariableReader;
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
} // namespace OCLRT
