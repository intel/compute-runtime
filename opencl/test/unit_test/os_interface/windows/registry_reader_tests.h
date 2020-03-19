/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/debug_registry_reader.h"

#include "opencl/source/os_interface/ocl_reg_path.h"

namespace NEO {
class TestedRegistryReader : public RegistryReader {
  public:
    TestedRegistryReader(bool userScope) : RegistryReader(userScope, oclRegPath){};
    TestedRegistryReader(std::string regKey) : RegistryReader(false, regKey){};
    HKEY getHkeyType() const {
        return hkeyType;
    }
    using RegistryReader::getSetting;

    char *getenv(const char *envVar) override {
        if (strcmp(envVar, "TestedEnvironmentVariable") == 0) {
            return "TestedEnvironmentVariableValue";
        } else if (strcmp(envVar, "TestedEnvironmentIntVariable") == 0) {
            return "1234";
        } else if (strcmp(envVar, "settingSourceString") == 0) {
            return "environment";
        } else if (strcmp(envVar, "settingSourceInt") == 0) {
            return "2";
        } else {
            return nullptr;
        }
    }
    const char *getRegKey() const {
        return registryReadRootKey.c_str();
    }
};
} // namespace NEO
