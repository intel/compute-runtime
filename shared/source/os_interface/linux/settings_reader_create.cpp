/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/debug_env_reader.h"

namespace NEO {

SettingsReader *SettingsReader::createOsReader(bool userScope, const std::string &regKey) {
    return new EnvironmentVariableReader;
}

char *SettingsReader::getenv(const char *settingName) {
    return ::getenv(settingName);
}
} // namespace NEO
