/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/debug_env_reader.h"

#include "shared/source/utilities/io_functions.h"

namespace NEO {

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

    envValue = IoFunctions::getenvPtr(settingName);
    if (envValue) {
        value = atoll(envValue);
    }
    return value;
}

std::string EnvironmentVariableReader::getSetting(const char *settingName, const std::string &value) {
    char *envValue;
    std::string keyValue;
    keyValue.assign(value);

    envValue = IoFunctions::getenvPtr(settingName);
    if (envValue) {
        keyValue.assign(envValue);
    }
    return keyValue;
}
} // namespace NEO
