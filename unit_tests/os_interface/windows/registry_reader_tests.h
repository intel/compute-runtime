/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace OCLRT;

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
