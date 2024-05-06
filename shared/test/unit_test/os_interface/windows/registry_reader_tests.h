/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/windows/debug_registry_reader.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"

namespace NEO {
class TestedRegistryReader : public RegistryReader {
  public:
    TestedRegistryReader(bool userScope) : RegistryReader(userScope, ApiSpecificConfig::getRegistryPath()) {
        initEnv();
    };
    TestedRegistryReader(std::string regKey) : RegistryReader(false, regKey) {
        initEnv();
    };
    HKEY getHkeyType() const {
        return hkeyType;
    }
    using RegistryReader::getSetting;
    using RegistryReader::processName;

    void initEnv() {

        IoFunctions::mockableEnvValues->insert({"TestedEnvironmentVariable", "TestedEnvironmentVariableValue"});
        IoFunctions::mockableEnvValues->insert({"NEO_TestedEnvironmentVariableWithPrefix", "TestedEnvironmentVariableValueWithPrefix"});
        IoFunctions::mockableEnvValues->insert({"TestedEnvironmentIntVariable", "1234"});
        IoFunctions::mockableEnvValues->insert({"NEO_TestedEnvironmentIntVariableWithPrefix", "5678"});
        IoFunctions::mockableEnvValues->insert({"TestedEnvironmentInt64Variable", "9223372036854775807"});
        IoFunctions::mockableEnvValues->insert({"NEO_TestedEnvironmentInt64VariableWithPrefix", "9223372036854775806"});
        IoFunctions::mockableEnvValues->insert({"settingSourceString", "environment"});
        IoFunctions::mockableEnvValues->insert({"settingSourceInt", "2"});
        IoFunctions::mockableEnvValues->insert({"processName", "processName"});
    }
    const char *getRegKey() const {
        return registryReadRootKey.c_str();
    }
    std::unordered_map<std::string, std::string> mockEnvironment{};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup{&IoFunctions::mockableEnvValues, &mockEnvironment};
};
} // namespace NEO
