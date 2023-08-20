/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/windows/debug_registry_reader.h"

namespace NEO {
class TestedRegistryReader : public RegistryReader {
  public:
    TestedRegistryReader(bool userScope) : RegistryReader(userScope, ApiSpecificConfig::getRegistryPath()){};
    TestedRegistryReader(std::string regKey) : RegistryReader(false, regKey){};
    HKEY getHkeyType() const {
        return hkeyType;
    }
    using RegistryReader::getSetting;
    using RegistryReader::processName;

    char *getenv(const char *envVar) override {
        if (strcmp(envVar, "TestedEnvironmentVariable") == 0) {
            return "TestedEnvironmentVariableValue";
        } else if (strcmp(envVar, "NEO_TestedEnvironmentVariableWithPrefix") == 0) {
            return "TestedEnvironmentVariableValueWithPrefix";
        } else if (strcmp(envVar, "TestedEnvironmentIntVariable") == 0) {
            return "1234";
        } else if (strcmp(envVar, "NEO_TestedEnvironmentIntVariableWithPrefix") == 0) {
            return "5678";
        } else if (strcmp(envVar, "TestedEnvironmentInt64Variable") == 0) {
            return "9223372036854775807";
        } else if (strcmp(envVar, "NEO_TestedEnvironmentInt64VariableWithPrefix") == 0) {
            return "9223372036854775806";
        } else if (strcmp(envVar, "settingSourceString") == 0) {
            return "environment";
        } else if (strcmp(envVar, "settingSourceInt") == 0) {
            return "2";
        } else if (strcmp(envVar, "processName") == 0) {
            return "processName";
        } else if (strcmp(envVar, "cl_cache_dir") == 0) {
            return "./tested_cl_cache_dir";
        } else {
            return nullptr;
        }
    }
    const char *getRegKey() const {
        return registryReadRootKey.c_str();
    }
};
} // namespace NEO
