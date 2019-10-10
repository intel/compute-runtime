/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/windows/debug_registry_reader.h"
#include "runtime/os_interface/ocl_reg_path.h"

namespace NEO {
class TestedRegistryReader : public RegistryReader {
  public:
    TestedRegistryReader(bool userScope) : RegistryReader(userScope, oclRegPath){};
    TestedRegistryReader(std::string regKey) : RegistryReader(false, regKey){};
    HKEY getHkeyType() const {
        return igdrclHkeyType;
    }
    using RegistryReader::getSetting;

    char *getenv(const char *envVar) override {
        if (strcmp(envVar, "TestedEnvironmentVariable") == 0) {
            return "TestedEnvironmentVariableValue";
        } else if (strcmp(envVar, "TestedEnvironmentIntVariable") == 0) {
            return "1234";
        } else {
            return nullptr;
        }
    }
    const char *getRegKey() const {
        return registryReadRootKey.c_str();
    }
};
} // namespace NEO
