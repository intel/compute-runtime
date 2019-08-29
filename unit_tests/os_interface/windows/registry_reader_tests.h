/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/windows/debug_registry_reader.h"

namespace NEO {
class TestedRegistryReader : public RegistryReader {
  public:
    TestedRegistryReader(bool userScope) : RegistryReader(userScope){};
    TestedRegistryReader(std::string regKey) : RegistryReader(regKey){};
    HKEY getHkeyType() const {
        return igdrclHkeyType;
    }
    using RegistryReader::getSetting;
    const char *getRegKey() const {
        return registryReadRootKey.c_str();
    }
};
} // namespace NEO
